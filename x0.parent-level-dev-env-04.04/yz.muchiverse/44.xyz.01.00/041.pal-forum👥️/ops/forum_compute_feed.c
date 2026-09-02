/* forum_compute_feed - PAL-FORUM-STANDARD.txt sec. 3, the real feed
 * algorithm (v1 scope, named explicitly - not a stub, not a full ML
 * ranking system):
 *
 *   1. Derive the CURRENT follow set by replaying <user_id>/
 *      following.txt's own FOLLOW/UNFOLLOW lines - the LAST line per
 *      target_user_id wins (matches pal-chain's own "replay the
 *      append-only log, never trust a mutable field" rule).
 *   2. Collect every POST this node has a LOCAL copy of (in
 *      users/<followed_id>/wall.txt, for every followed_id) - a
 *      node's own feed can only ever include posts it has actually
 *      received over the mesh (or authored itself), never posts it
 *      has no local copy of at all.
 *   3. Sort by recency (newer first); same-timestamp tiebreak by
 *      like_count - counted per post_id across EVERY user directory
 *      this node currently has a local likes.txt for (not just the
 *      followed set - a like from someone you don't follow still
 *      counts, matching the spec's own "across every peer's own copy
 *      this node has seen" wording).
 *   4. Write the result to users/<user_id>/feed_cache.txt - a derived,
 *      regenerable cache, never authoritative (matching pal-chain's
 *      own cached_balance precedent - re-running this op always
 *      rebuilds it from the real source files, never trusts a stale
 *      copy).
 *
 * PAL-VS-C-ARCHITECTURE.txt sec. 4's own named bad-PAL-candidate shape
 * (an unbounded directory scan/sort) - stays C, per PAL-FORUM-STANDARD
 * sec. 5's own explicit call-out.
 *
 * Self-contained, no shared headers.
 * Usage: forum_compute_feed.+x <user_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_FOLLOWED 256
#define MAX_POSTS 4096

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

typedef struct {
    char post_id[128];
    char author[64];
    long timestamp;
    char text[MAX_LINE];
    char image_id[64];
    long like_count;
} Post;

static char g_followed[MAX_FOLLOWED][64];
static int g_followed_count = 0;

static int is_followed(const char *id) {
    for (int i = 0; i < g_followed_count; i++) if (strcmp(g_followed[i], id) == 0) return 1;
    return 0;
}

static void remove_followed(const char *id) {
    for (int i = 0; i < g_followed_count; i++) {
        if (strcmp(g_followed[i], id) == 0) {
            for (int j = i; j < g_followed_count - 1; j++) snprintf(g_followed[j], sizeof(g_followed[j]), "%s", g_followed[j + 1]);
            g_followed_count--;
            return;
        }
    }
}

/* Replays FOLLOW/UNFOLLOW lines in file order - the LAST verb seen for
 * any given target_user_id determines whether they're in the current
 * follow set, exactly matching how a later UNFOLLOW line cancels an
 * earlier FOLLOW without rewriting it. */
static void derive_follow_set(const char *following_path) {
    FILE *f = fopen(following_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line);
        char *fields[4]; int nf = 0; char *cursor = copy;
        for (; nf < 3; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 3) continue;
        const char *verb = fields[0];
        const char *target = fields[2];
        if (strcmp(verb, "FOLLOW") == 0) {
            if (!is_followed(target) && g_followed_count < MAX_FOLLOWED) {
                snprintf(g_followed[g_followed_count], sizeof(g_followed[g_followed_count]), "%s", target);
                g_followed_count++;
            }
        } else if (strcmp(verb, "UNFOLLOW") == 0) {
            remove_followed(target);
        }
    }
    fclose(f);
}

/* Counts LIKE lines for post_id across EVERY users/<id>/likes.txt this
 * node currently has - a full directory scan per post is real, but
 * v1-acceptable at this scale (matching the spec's own "not a full ML
 * ranking system on day one" framing). */
static long count_likes(const char *post_id) {
    long count = 0;
    char users_root[PATH_BUF];
    snprintf(users_root, sizeof(users_root), "%s/users", project_root);
    DIR *d = opendir(users_root);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char likes_path[PATH_BUF];
        snprintf(likes_path, sizeof(likes_path), "%s/%s/likes.txt", users_root, ent->d_name);
        FILE *f = fopen(likes_path, "r");
        if (!f) continue;
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            char copy[MAX_LINE];
            snprintf(copy, sizeof(copy), "%s", line);
            char *fields[4]; int nf = 0; char *cursor = copy;
            for (; nf < 3; nf++) {
                char *pipe = strchr(cursor, '|');
                if (!pipe) break;
                *pipe = '\0';
                fields[nf] = cursor;
                cursor = pipe + 1;
            }
            if (nf < 3) continue;
            if (strcmp(fields[2], post_id) == 0) count++;
        }
        fclose(f);
    }
    closedir(d);
    return count;
}

static Post g_posts[MAX_POSTS];
static int g_post_count = 0;

static void collect_wall_posts(const char *author_id) {
    char wall_path[PATH_BUF];
    snprintf(wall_path, sizeof(wall_path), "%s/users/%s/wall.txt", project_root, author_id);
    FILE *f = fopen(wall_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && g_post_count < MAX_POSTS) {
        line[strcspn(line, "\n")] = '\0';
        char copy[MAX_LINE];
        snprintf(copy, sizeof(copy), "%s", line);
        /* POST|<post_id>|<user_id>|<timestamp>|<text>|<image_id_or_-> */
        char *fields[6]; int nf = 0; char *cursor = copy;
        for (; nf < 5; nf++) {
            char *pipe = strchr(cursor, '|');
            if (!pipe) break;
            *pipe = '\0';
            fields[nf] = cursor;
            cursor = pipe + 1;
        }
        if (nf < 5) continue;
        if (strcmp(fields[0], "POST") != 0) continue;
        Post *p = &g_posts[g_post_count];
        snprintf(p->post_id, sizeof(p->post_id), "%s", fields[1]);
        snprintf(p->author, sizeof(p->author), "%s", fields[2]);
        p->timestamp = atol(fields[3]);
        snprintf(p->text, sizeof(p->text), "%s", fields[4]);
        snprintf(p->image_id, sizeof(p->image_id), "%s", cursor);
        p->like_count = count_likes(p->post_id);
        g_post_count++;
    }
    fclose(f);
}

static int compare_posts(const void *a, const void *b) {
    const Post *pa = (const Post *)a;
    const Post *pb = (const Post *)b;
    if (pa->timestamp != pb->timestamp) return (pb->timestamp > pa->timestamp) ? 1 : -1;
    if (pa->like_count != pb->like_count) return (pb->like_count > pa->like_count) ? 1 : -1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: forum_compute_feed.+x <user_id>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) { fprintf(stderr, "No such user '%s'.\n", user_id); return 1; }

    char following_path[PATH_BUF];
    snprintf(following_path, sizeof(following_path), "%s/following.txt", user_dir);
    derive_follow_set(following_path);

    for (int i = 0; i < g_followed_count; i++) collect_wall_posts(g_followed[i]);

    qsort(g_posts, (size_t)g_post_count, sizeof(Post), compare_posts);

    char cache_path[PATH_BUF];
    snprintf(cache_path, sizeof(cache_path), "%s/feed_cache.txt", user_dir);
    FILE *out = fopen(cache_path, "w");
    if (!out) { fprintf(stderr, "Could not write feed_cache.\n"); return 1; }
    for (int i = 0; i < g_post_count; i++) {
        Post *p = &g_posts[i];
        fprintf(out, "POST|%s|%s|%ld|%s|%s|%ld\n", p->post_id, p->author, p->timestamp, p->text, p->image_id, p->like_count);
    }
    fclose(out);

    printf("Feed computed: %d posts from %d followed users.\n", g_post_count, g_followed_count);
    return 0;
}
