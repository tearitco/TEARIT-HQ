#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define EMBEDDING_DIM 7
#define HIDDEN_DIM 16

// --- Data Structures ---
struct VocabEntry { int number; char word[100]; float embedding, pe, weight, bias1, bias2, bias3, bias4; };
typedef struct { float weights[EMBEDDING_DIM][HIDDEN_DIM]; float biases[HIDDEN_DIM]; } MlpLayer;
typedef struct { float W_q[EMBEDDING_DIM][EMBEDDING_DIM], W_k[EMBEDDING_DIM][EMBEDDING_DIM], W_v[EMBEDDING_DIM][EMBEDDING_DIM]; } AttentionLayer;
typedef struct { float **weights; float *biases; } OutputLayer;

// --- File I/O ---
void save_matrix(const char *fn, float *m, int rows, int cols) { FILE *f=fopen(fn,"w"); if(f){ for(int i=0;i<rows;i++){ for(int j=0;j<cols;j++) fprintf(f, "%.9g ", m[i*cols+j]); fprintf(f, "\n"); } fclose(f); fprintf(stderr, "Saved matrix to %s\n", fn); } }

// Load matrices from human-readable text files (same format the optimizer writes).
void load_attention_text(const char *fn, AttentionLayer *l) {
    FILE *f=fopen(fn,"r"); if(!f){ fprintf(stderr,"Warning: could not open %s\n",fn); return; }
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f,"%f",&l->W_q[i][j]);
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f,"%f",&l->W_k[i][j]);
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f,"%f",&l->W_v[i][j]);
    fclose(f);
}
void load_mlp_text(const char *fn, MlpLayer *l) {
    FILE *f=fopen(fn,"r"); if(!f){ fprintf(stderr,"Warning: could not open %s\n",fn); return; }
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<HIDDEN_DIM;j++) fscanf(f,"%f",&l->weights[i][j]);
    for(int i=0;i<HIDDEN_DIM;i++) fscanf(f,"%f",&l->biases[i]);
    fclose(f);
}
void load_output_text(const char *fn, OutputLayer *l, int vs) {
    FILE *f=fopen(fn,"r"); if(!f){ fprintf(stderr,"Warning: could not open %s\n",fn); return; }
    for(int i=0;i<HIDDEN_DIM;i++) for(int j=0;j<vs;j++) fscanf(f,"%f",&l->weights[i][j]);
    for(int i=0;i<vs;i++) fscanf(f,"%f",&l->biases[i]);
    fclose(f);
}

// --- Utility Functions ---
// Simple dropout implementation
void apply_dropout(float *x, int size, float dropout_rate) {
    // Skip dropout if rate is invalid or zero
    if (dropout_rate <= 0.0f || dropout_rate >= 1.0f) return;
    
    // For reproducibility, we'll use a simple linear congruential generator
    static unsigned int seed = 12345;
    
    for (int i = 0; i < size; i++) {
        // Generate pseudo-random number between 0 and 1
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        float rand_val = (float)seed / (float)0x7fffffff;
        
        // Apply dropout
        if (rand_val < dropout_rate) {
            x[i] = 0.0f;  // Zero out
        } else {
            x[i] /= (1.0f - dropout_rate);  // Scale remaining values
        }
    }
}

// Apply causal mask to attention scores
void apply_causal_mask(float *attn_scores, int vocab_size, int current_position) {
    for (int i = current_position + 1; i < vocab_size; i++) {
        attn_scores[i] = -1e9f; // Set future positions to negative infinity
    }
}

// Layer normalization
void layer_norm(float *x, int size) {
    // Calculate mean
    float mean = 0.0f;
    for (int i = 0; i < size; i++) {
        // Check for valid values
        if (x[i] != x[i] || x[i] > 1e10f || x[i] < -1e10f) {
            x[i] = 0.0f;
        }
        mean += x[i];
    }
    mean /= size;
    
    // Calculate variance
    float variance = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = x[i] - mean;
        variance += diff * diff;
    }
    variance /= size;
    
    // Prevent division by zero
    float std_dev = sqrtf(variance + 1e-8f);
    
    // Normalize
    for (int i = 0; i < size; i++) {
        x[i] = (x[i] - mean) / std_dev;
    }
}

// --- Forward Pass Functions ---
void relu(float *x, int s){for(int i=0;i<s;i++)if(x[i]<0)x[i]=0;}
void softmax(float *x, int s){
    // Improve numerical stability with better max finding and overflow protection
    float max = x[0];
    for(int i = 1; i < s; i++) {
        if(x[i] > max) max = x[i];
    }
    
    float sum = 0.0f;
    for(int i = 0; i < s; i++) {
        // Calculate exponent with numerical stability
        float val = x[i] - max;
        // Prevent extreme values that cause overflow/underflow
        if (val > 88.0f) val = 88.0f;    // Prevent overflow (exp(88) is near float max)
        if (val < -88.0f) val = -88.0f;  // Prevent underflow
        x[i] = expf(val);
        sum += x[i];
    }
    
    // Handle case where sum is zero, very small, or NaN/Inf
    if (sum != sum || sum < 1e-15f || sum > 1e15f) {  // Check for NaN or extreme values
        // Use uniform distribution
        for(int i = 0; i < s; i++) {
            x[i] = 1.0f / s;
        }
    } else {
        // Normalize
        for(int i = 0; i < s; i++) {
            x[i] /= sum;
            // Final safety check for NaN/Inf
            if (x[i] != x[i] || x[i] > 1e10f || x[i] < -1e10f) {
                x[i] = 1.0f / s;  // Replace with uniform value
            }
        }
    }
}

// --- Main ---
int main(int argc, char *argv[]) {
    if (argc < 6) { fprintf(stderr, "Usage: %s <vocab> <word_idx> <attn_model> <mlp_model> <out_model> [causal_attention]\n", argv[0]); return 1; }
    char *od=dirname(strdup(argv[1])); 
    
    // Check if causal attention is enabled (default is 0/disabled)
    int causal_attention = 0;
    if (argc >= 7) {
        causal_attention = atoi(argv[6]);
    }
    
    // Dynamically allocate vocab array
    struct VocabEntry *v = NULL;
    int vs = 0;
    int capacity = 1000; // Start with reasonable capacity
    v = malloc(capacity * sizeof(struct VocabEntry));
    if (!v) {
        fprintf(stderr, "Failed to allocate memory for vocab\n");
        return 1;
    }
    
    FILE *vf=fopen(argv[1],"r"); 
    if(vf){
        char l[MAX_LINE_LENGTH]; 
        fgets(l,sizeof(l),vf); // Skip header
        while(fgets(l,sizeof(l),vf)) {
            // Expand array if needed
            if (vs >= capacity) {
                capacity *= 2;
                struct VocabEntry *temp = realloc(v, capacity * sizeof(struct VocabEntry));
                if (!temp) {
                    fprintf(stderr, "Failed to reallocate memory for vocab\n");
                    free(v);
                    fclose(vf);
                    return 1;
                }
                v = temp;
            }
            
            // Parse line
            int result = sscanf(l,"%d %s %f %f %f %f %f %f %f",
                               &v[vs].number,v[vs].word,&v[vs].embedding,&v[vs].pe,&v[vs].weight,
                               &v[vs].bias1,&v[vs].bias2,&v[vs].bias3,&v[vs].bias4);
            if (result == 9) { // Successfully parsed all fields
                vs++;
            }
        }
        fclose(vf);
    } else {
        fprintf(stderr, "Failed to open vocab file: %s\n", argv[1]);
        free(v);
        return 1;
    }
    
    int wi=atoi(argv[2]); 
    if(wi<0||wi>=vs){
        fprintf(stderr,"Invalid word index: %d (vocab size: %d)\n", wi, vs);
        free(v);
        return 1;
    }

    AttentionLayer a; MlpLayer m; OutputLayer o;
    load_attention_text(argv[3], &a);
    load_mlp_text(argv[4], &m);
    o.weights=malloc(HIDDEN_DIM*sizeof(float*)); for(int i=0;i<HIDDEN_DIM;i++)o.weights[i]=malloc(vs*sizeof(float)); o.biases=malloc(vs*sizeof(float));
    load_output_text(argv[5], &o, vs);

    // Dynamically allocate all buffers
float *iv=malloc(EMBEDDING_DIM*sizeof(float));
float *q=malloc(EMBEDDING_DIM*sizeof(float));
float *k=malloc(EMBEDDING_DIM*sizeof(float));
float *val=malloc(EMBEDDING_DIM*sizeof(float));
float *ctx=malloc(EMBEDDING_DIM*sizeof(float));
float *h=malloc(HIDDEN_DIM*sizeof(float));
float *as=malloc(vs*sizeof(float));
float *p=malloc(vs*sizeof(float));

// Initialize input vector
iv[0]=v[wi].embedding; iv[1]=v[wi].pe; iv[2]=v[wi].weight; iv[3]=v[wi].bias1; 
iv[4]=v[wi].bias2; iv[5]=v[wi].bias3; iv[6]=v[wi].bias4;

// Initialize arrays to zero
for(int j=0;j<EMBEDDING_DIM;j++) q[j]=k[j]=val[j]=ctx[j]=0;
for(int j=0;j<HIDDEN_DIM;j++) h[j]=0;
for(int j=0;j<vs;j++) as[j]=p[j]=0;

for(int j=0;j<EMBEDDING_DIM;j++){
    for(int l=0;l<EMBEDDING_DIM;l++){
        // Check for valid values before computation
        if (iv[l] != iv[l] || a.W_q[l][j] != a.W_q[l][j] || 
            iv[l] > 1e10f || iv[l] < -1e10f || 
            a.W_q[l][j] > 1e10f || a.W_q[l][j] < -1e10f) {
            continue;  // Skip invalid values
        }
        q[j]+=iv[l]*a.W_q[l][j];
        
        if (iv[l] != iv[l] || a.W_k[l][j] != a.W_k[l][j] || 
            iv[l] > 1e10f || iv[l] < -1e10f || 
            a.W_k[l][j] > 1e10f || a.W_k[l][j] < -1e10f) {
            continue;  // Skip invalid values
        }
        k[j]+=iv[l]*a.W_k[l][j];
        
        if (iv[l] != iv[l] || a.W_v[l][j] != a.W_v[l][j] || 
            iv[l] > 1e10f || iv[l] < -1e10f || 
            a.W_v[l][j] > 1e10f || a.W_v[l][j] < -1e10f) {
            continue;  // Skip invalid values
        }
        val[j]+=iv[l]*a.W_v[l][j];
    }
}
    float scale = 1.0f / sqrt(EMBEDDING_DIM);
    for(int j=0;j<vs;j++){
        float nk[EMBEDDING_DIM]={v[j].embedding,v[j].pe,v[j].weight,v[j].bias1,v[j].bias2,v[j].bias3,v[j].bias4}; 
        as[j]=0; 
        for(int l=0;l<EMBEDDING_DIM;l++)as[j]+=q[l]*nk[l]; 
        as[j]*=scale;
        // Additional clipping to prevent large values
        if (as[j] > 10.0f) as[j] = 10.0f;
        if (as[j] < -10.0f) as[j] = -10.0f;
    }
    char pth[1024]; sprintf(pth,"%s/attn_scores_raw.txt",od); save_matrix(pth,as,1,vs);
    
    // Apply causal mask if enabled
    if (causal_attention) {
        apply_causal_mask(as, vs, wi);
    }
    
    softmax(as,vs);
    
    for(int l=0;l<EMBEDDING_DIM;l++){ctx[l]=0; for(int j=0;j<vs;j++){float nv[EMBEDDING_DIM]={v[j].embedding,v[j].pe,v[j].weight,v[j].bias1,v[j].bias2,v[j].bias3,v[j].bias4}; ctx[l]+=as[j]*nv[l];}}
    
    // Apply residual connection (add input to context)
    for(int l=0;l<EMBEDDING_DIM;l++) {
        ctx[l] += iv[l];  // Residual connection
    }
    
    for(int j=0;j<HIDDEN_DIM;j++){h[j]=0; for(int l=0;l<EMBEDDING_DIM;l++)h[j]+=ctx[l]*m.weights[l][j]; h[j]+=m.biases[j];} 
    
    relu(h,HIDDEN_DIM);
    
    for(int j=0;j<vs;j++){p[j]=0; for(int l=0;l<HIDDEN_DIM;l++)p[j]+=h[l]*o.weights[l][j]; p[j]+=o.biases[j];}

    sprintf(pth,"%s/q.txt",od); save_matrix(pth,q,1,EMBEDDING_DIM);
    sprintf(pth,"%s/k.txt",od); save_matrix(pth,k,1,EMBEDDING_DIM);
    sprintf(pth,"%s/v.txt",od); save_matrix(pth,val,1,EMBEDDING_DIM);
    sprintf(pth,"%s/attn_scores.txt",od); save_matrix(pth,as,1,vs);
    sprintf(pth,"%s/context.txt",od); save_matrix(pth,ctx,1,EMBEDDING_DIM);
    sprintf(pth,"%s/hidden_state.txt",od); save_matrix(pth,h,1,HIDDEN_DIM);
    sprintf(pth,"%s/predictions.txt",od); save_matrix(pth,p,1,vs);

    // Cleanup allocated memory
    free(iv);
    free(q);
    free(k);
    free(val);
    free(ctx);
    free(h);
    free(as);
    free(p);
    for(int i=0; i<HIDDEN_DIM; i++) {
        free(o.weights[i]);
    }
    free(o.weights);
    free(o.biases);
    free(v);

    fprintf(stderr, "Forward propagation completed.\n");
    return 0;
}