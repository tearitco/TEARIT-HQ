🌳 C-Gitlet 🌳🪵️do we trunk yet? 🌳🪵️
A simple version control system inspired by Git, written in pure C.
🤔 How to Use 🤔

Compile the code:
gcc gitlet.c -o gitlet


Initialize a repository:
./gitlet init

Or for a bare repository (useful for remotes):
./gitlet init --bare


Add files to the index:
./gitlet add <file>


Commit your changes:
./gitlet commit "<message>"


View commit history:
./gitlet log


Check status:
./gitlet status


Create or list branches:
./gitlet branch <branch-name>  # create
./gitlet branch  # list


Switch branches:
./gitlet checkout <branch-name>


Remove a file:
./gitlet rm <file>


Push to remote:
./gitlet push <remote_path>


Pull from remote:
./gitlet pull <remote_path>



🤝 Similarities to Git 🤝



C-Gitlet Command
Git Command



gitlet init
git init


gitlet init --bare
git init --bare


gitlet add
git add


gitlet commit
git commit


gitlet log
git log


gitlet status
git status


gitlet branch
git branch


gitlet checkout
git checkout


gitlet rm
git rm


gitlet push <path>
git push


gitlet pull <path>
git pull


🚀 New Features 🚀
🌟 We've supercharged C-Gitlet with push and pull commands to handle remote repositories! 📡🔄

Push: 🚀 Sends your local commits to a remote repository path. Similar to Git's push, but uses a direct file path instead of a named remote. Great for sharing changes! 💾

Pull: 🔄 Fetches commits from a remote and updates your local branch and working directory. Like Git's pull, but simplified—no merging, just overwrites with remote state. ⚡


These features make C-Gitlet more collaborative, allowing you to simulate remote workflows locally! 🤝 Compare to Git: While Git uses URLs or named remotes, here we keep it simple with local paths for educational fun. 🎓
📚 Detailed Guide for Beginners 📚
Hey newbie! 👋 Welcome to C-Gitlet—the fun, emoji-filled way to learn version control! 🌈 Let's break it down step by step with lots of sparkles and tips. ✨
🛠️ Setting Up Your First Repo 🛠️

Compile the magic: 🔨 Run gcc gitlet.c -o gitlet to build the tool. Boom—ready to rock! 🎸

Init time! 🚀 ./gitlet init creates a shiny new repo in .gitlet/. This is like your project's time machine! ⏳

Pro tip: For a "bare" repo (no working files, perfect for servers/remotes), use ./gitlet init --bare. 🗄️



📁 Adding and Committing Files 📁

Create a file: 📝 echo "Hello, world!" > hello.txt—your first masterpiece! 🎨

Add it: ➕ ./gitlet add hello.txt stages it for commit. Think of this as putting it in a gift box! 🎁

Commit: ✅ ./gitlet commit "My first change" saves it forever. Add a message to remember why! 🧠


🔍 Checking Things Out 🔍

Status check: 🕵️ ./gitlet status shows what's up—branches, staged files, untracked stuff. Your repo's health report! 💉

Log history: 📜 ./gitlet log lists commits. Travel back in time! 🕰️


🌿 Branching Like a Pro 🌿

Create branch: 🌱 ./gitlet branch cool-feature makes a new path for experiments. No breaking main! ⚠️

List branches: 📋 ./gitlet branch shows all. Pick your adventure! 🗺️

Switch: 🔄 ./gitlet checkout cool-feature jumps to it. Files update magically! 🪄


🗑️ Removing Stuff 🗑️

Rm file: ❌ ./gitlet rm hello.txt unstages and deletes it. Bye-bye! 👋

🌐 Going Remote with Push & Pull 🌐

Set up remote: 🏰 mkdir my-remote then cd my-remote && ../gitlet init --bare cd back. Your far-away castle! 🏰

Push changes: 📤 From your local repo: ./gitlet push ../my-remote sends commits over. Share the love! ❤️

Emoji alert: If it works, you've just "pushed" like a Git boss! 💪 (But remember, this is simple—no conflict checks. 😅)


Pull updates: 📥 ./gitlet pull ../my-remote grabs remote changes and applies them. Stay in sync! 🔄

Fun fact: This overwrites local with remote—use carefully! ⚠️ Like Git pull but super basic. 🎉



💡 Tips for Newbs 💡

Always check status before committing! 👀
Branches are cheap—use them for features! 🌟
For remotes, paths are local dirs. No internet needed! 🏠
If stuff breaks, peek inside .gitlet/—it's all files! 🔍
Test everything with gcc test.c -o test && ./test to see passes/fails. ✅❌

Dive in, experiment, and have fun versioning! 🚀 If you mess up, just init a new one. 😄 Happy coding! 👩‍💻👨‍💻
