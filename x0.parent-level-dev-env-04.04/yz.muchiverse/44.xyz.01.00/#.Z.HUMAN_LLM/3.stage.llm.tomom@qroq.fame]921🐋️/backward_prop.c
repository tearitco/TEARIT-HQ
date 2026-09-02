#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <libgen.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VOCAB_SIZE 100000
#define EMBEDDING_DIM 7
#define HIDDEN_DIM 16

// --- Data Structures ---
struct VocabEntry { int number; char word[100]; float embedding, pe, weight, bias1, bias2, bias3, bias4; };
typedef struct { float weights[EMBEDDING_DIM][HIDDEN_DIM]; float biases[HIDDEN_DIM]; } MlpLayer;
typedef struct { float W_q[EMBEDDING_DIM][EMBEDDING_DIM], W_k[EMBEDDING_DIM][EMBEDDING_DIM], W_v[EMBEDDING_DIM][EMBEDDING_DIM]; } AttentionLayer;
typedef struct { float **weights, *biases; } OutputLayer;

// --- File I/O ---
void load_matrix(const char *fn, float *m, int r, int c) { FILE *f=fopen(fn,"r"); if(f){ for(int i=0;i<r;i++) for(int j=0;j<c;j++) fscanf(f, "%f", &m[i*c+j]); fclose(f); } }
void save_matrix(const char *fn, float *m, int r, int c) { FILE *f=fopen(fn,"w"); if(f){ for(int i=0;i<r;i++){ for(int j=0;j<c;j++) fprintf(f, "%.9g ", m[i*c+j]); fprintf(f, "\n"); } fclose(f); fprintf(stderr, "Saved matrix to %s\n", fn); } }

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

// Save gradient for the output layer (jagged float** rows + biases) in optimizer text format.
void save_grad_output(const char *fn, OutputLayer *l, int vs) {
    FILE *f=fopen(fn,"w"); if(!f){ fprintf(stderr,"Warning: could not open %s\n",fn); return; }
    for(int i=0;i<HIDDEN_DIM;i++){ for(int j=0;j<vs;j++) fprintf(f,"%.9g ",l->weights[i][j]); fprintf(f,"\n"); }
    for(int j=0;j<vs;j++) fprintf(f,"%.9g ",l->biases[j]); fprintf(f,"\n");
    fclose(f); fprintf(stderr,"Saved gradient output to %s\n",fn);
}
// Save gradient for the MLP layer (contiguous weights + biases) in optimizer text format.
void save_grad_mlp(const char *fn, MlpLayer *l) {
    FILE *f=fopen(fn,"w"); if(!f){ fprintf(stderr,"Warning: could not open %s\n",fn); return; }
    for(int i=0;i<EMBEDDING_DIM;i++){ for(int j=0;j<HIDDEN_DIM;j++) fprintf(f,"%.9g ",l->weights[i][j]); fprintf(f,"\n"); }
    for(int i=0;i<HIDDEN_DIM;i++) fprintf(f,"%.9g ",l->biases[i]); fprintf(f,"\n");
    fclose(f); fprintf(stderr,"Saved gradient MLP to %s\n",fn);
}

// --- Backward Pass Functions ---
void relu_derivative(float *x, float *d, int s){ 
    for(int i=0;i<s;i++) {
        // Safety check for NaN/Inf
        if (x[i] != x[i] || x[i] > 1e10f || x[i] < -1e10f) {
            d[i] = 0;  // Set to zero for invalid values
        } else {
            d[i] = (x[i] > 0) ? 1 : 0;
        }
    }
}

// Calculate L2 norm of gradients with NaN/Inf checking
float gradient_norm(float *grad, int size) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        // Check for NaN or Inf values
        if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
            grad[i] = 0.0f;  // Reset invalid values
            continue;
        }
        sum += grad[i] * grad[i];
    }
    // Prevent sqrt of negative numbers
    if (sum < 0) sum = 0;
    return sqrtf(sum);
}

// Clip gradients to prevent explosion with NaN/Inf checking
void clip_gradients(float *grad, int size, float max_norm) {
    // First check for NaN/Inf and reset them
    for (int i = 0; i < size; i++) {
        if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
            grad[i] = 0.0f;
        }
    }
    
    float norm = gradient_norm(grad, size);
    if (norm > max_norm && norm > 1e-12f) {  // Add epsilon check
        float scale = max_norm / norm;
        // Ensure scale is reasonable
        if (scale != scale || scale > 1e10f || scale < -1e10f) {
            scale = 1.0f;
        }
        for (int i = 0; i < size; i++) {
            grad[i] *= scale;
            // Final safety check
            if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
                grad[i] = 0.0f;
            }
        }
    }
}

// Clip gradients for 2D arrays with NaN/Inf checking
void clip_gradients_2d(float *grad, int rows, int cols, float max_norm) {
    int size = rows * cols;
    // First check for NaN/Inf and reset them
    for (int i = 0; i < size; i++) {
        if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
            grad[i] = 0.0f;
        }
    }
    
    float norm = 0.0f;
    for (int i = 0; i < size; i++) {
        norm += grad[i] * grad[i];
    }
    norm = sqrtf(norm);
    
    if (norm > max_norm && norm > 1e-12f) {  // Add epsilon check
        float scale = max_norm / norm;
        // Ensure scale is reasonable
        if (scale != scale || scale > 1e10f || scale < -1e10f) {
            scale = 1.0f;
        }
        for (int i = 0; i < size; i++) {
            grad[i] *= scale;
            // Final safety check
            if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
                grad[i] = 0.0f;
            }
        }
    }
}

// Add gradient noise for better generalization
void add_gradient_noise(float *grad, int size, float noise_scale) {
    // Skip if noise scale is invalid or zero
    if (noise_scale <= 0.0f) return;
    
    // For reproducibility, we'll use a simple linear congruential generator
    static unsigned int seed = 54321;
    
    for (int i = 0; i < size; i++) {
        // Generate pseudo-random number from normal distribution (approximation)
        // Box-Muller transform approximation
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        float u1 = (float)seed / (float)0x7fffffff;
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        float u2 = (float)seed / (float)0x7fffffff;
        
        // Prevent log(0)
        if (u1 < 1e-10f) u1 = 1e-10f;
        
        // Generate normal random variable
        float noise = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
        
        // Scale noise
        noise *= noise_scale;
        
        // Add noise to gradient with safety check
        if (grad[i] != grad[i] || grad[i] > 1e10f || grad[i] < -1e10f) {
            grad[i] = noise;  // Replace invalid gradient with noise
        } else {
            grad[i] += noise;
        }
    }
}

// --- Main ---
int main(int argc, char *argv[]) {
    if (argc < 13) { fprintf(stderr, "Usage: %s <vocab> <word_idx> <grad_loss> <attn_model> <mlp_model> <out_model> <hidden> <context> <q> <k> <v> <attn_scores_raw>\n", argv[0]); return 1; }
    char *od=dirname(strdup(argv[1])); 
    
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

    AttentionLayer a; MlpLayer m; OutputLayer o;
    load_attention_text(argv[4], &a);
    load_mlp_text(argv[5], &m);
    o.weights=malloc(HIDDEN_DIM*sizeof(float*)); for(int i=0;i<HIDDEN_DIM;i++)o.weights[i]=malloc(vs*sizeof(float)); o.biases=malloc(vs*sizeof(float));
    load_output_text(argv[6], &o, vs);

    float *gl=malloc(vs*sizeof(float)), *h=malloc(HIDDEN_DIM*sizeof(float)), *c=malloc(EMBEDDING_DIM*sizeof(float));
    float *q=malloc(EMBEDDING_DIM*sizeof(float)), *k=malloc(EMBEDDING_DIM*sizeof(float)), *val=malloc(EMBEDDING_DIM*sizeof(float)), *asr=malloc(vs*sizeof(float));
    load_matrix(argv[3],gl,1,vs); load_matrix(argv[7],h,1,HIDDEN_DIM); load_matrix(argv[8],c,1,EMBEDDING_DIM);
    load_matrix(argv[9],q,1,EMBEDDING_DIM); load_matrix(argv[10],k,1,EMBEDDING_DIM); load_matrix(argv[11],val,1,EMBEDDING_DIM); load_matrix(argv[12],asr,1,vs);

    float *g_p=gl, *g_h=malloc(HIDDEN_DIM*sizeof(float)), *g_c=malloc(EMBEDDING_DIM*sizeof(float)), *g_iv=malloc(EMBEDDING_DIM*sizeof(float));
    OutputLayer g_o; g_o.weights=malloc(HIDDEN_DIM*sizeof(float*)); for(int i=0;i<HIDDEN_DIM;i++)g_o.weights[i]=malloc(vs*sizeof(float)); g_o.biases=malloc(vs*sizeof(float));
    MlpLayer g_m; AttentionLayer g_a;
    
    // Initialize gradient arrays to zero
    for(int i=0;i<HIDDEN_DIM;i++) g_h[i]=0;
    for(int i=0;i<EMBEDDING_DIM;i++) g_c[i]=g_iv[i]=0;
    
    // Initialize gradients to zero to prevent undefined behavior
    for(int i=0;i<HIDDEN_DIM;i++) for(int j=0;j<vs;j++) g_o.weights[i][j]=0;
    for(int j=0;j<vs;j++) g_o.biases[j]=0;
    for(int i=0;i<HIDDEN_DIM;i++) g_h[i]=0;
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<HIDDEN_DIM;j++) g_m.weights[i][j]=0;
    for(int j=0;j<HIDDEN_DIM;j++) g_m.biases[j]=0;
    for(int i=0;i<EMBEDDING_DIM;i++) g_c[i]=0;
    for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) g_a.W_q[i][j]=0;

    // Compute output layer gradients with NaN/Inf checks
    for(int i=0;i<HIDDEN_DIM;i++) {
        for(int j=0;j<vs;j++) {
            // Check for valid values before computation
            if (g_p[j] != g_p[j] || h[i] != h[i] || g_p[j] > 1e10f || g_p[j] < -1e10f || h[i] > 1e10f || h[i] < -1e10f) {
                g_o.weights[i][j] = 0;
            } else {
                g_o.weights[i][j] = g_p[j] * h[i];
            }
            // Additional clipping to prevent large values
            if (g_o.weights[i][j] > 10.0f) g_o.weights[i][j] = 10.0f;
            if (g_o.weights[i][j] < -10.0f) g_o.weights[i][j] = -10.0f;
        }
    }
    
    for(int j=0;j<vs;j++) {
        if (g_p[j] != g_p[j] || g_p[j] > 1e10f || g_p[j] < -1e10f) {
            g_o.biases[j] = 0;
        } else {
            g_o.biases[j] = g_p[j];
        }
        // Additional clipping to prevent large values
        if (g_o.biases[j] > 10.0f) g_o.biases[j] = 10.0f;
        if (g_o.biases[j] < -10.0f) g_o.biases[j] = -10.0f;
    }
    
    // Compute hidden layer gradients
    for(int i=0;i<HIDDEN_DIM;i++){
        g_h[i]=0; 
        for(int j=0;j<vs;j++) {
            // Check for valid values
            if (g_p[j] != g_p[j] || o.weights[i][j] != o.weights[i][j] || 
                g_p[j] > 1e10f || g_p[j] < -1e10f || 
                o.weights[i][j] > 1e10f || o.weights[i][j] < -1e10f) {
                continue;  // Skip invalid values
            }
            g_h[i] += g_p[j]*o.weights[i][j];
        }
    }
    
    // Apply ReLU derivative with safety checks
    float d_r[HIDDEN_DIM]; 
    relu_derivative(h,d_r,HIDDEN_DIM); 
    for(int j=0;j<HIDDEN_DIM;j++) {
        // Check for valid values before multiplication
        if (g_h[j] != g_h[j] || d_r[j] != d_r[j] || g_h[j] > 1e10f || g_h[j] < -1e10f) {
            g_h[j] = 0;
        } else {
            g_h[j] *= d_r[j];
        }
    }
    
    // Compute MLP layer gradients
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<HIDDEN_DIM;j++) {
            // Check for valid values
            if (g_h[j] != g_h[j] || c[i] != c[i] || g_h[j] > 1e10f || g_h[j] < -1e10f || c[i] > 1e10f || c[i] < -1e10f) {
                g_m.weights[i][j] = 0;
            } else {
                g_m.weights[i][j] = g_h[j] * c[i];
            }
        }
    }
    
    for(int j=0;j<HIDDEN_DIM;j++) {
        if (g_h[j] != g_h[j] || g_h[j] > 1e10f || g_h[j] < -1e10f) {
            g_m.biases[j] = 0;
        } else {
            g_m.biases[j] = g_h[j];
        }
    }
    
    // Compute context gradients
    for(int i=0;i<EMBEDDING_DIM;i++){
        g_c[i]=0; 
        for(int j=0;j<HIDDEN_DIM;j++) {
            // Check for valid values
            if (g_h[j] != g_h[j] || m.weights[i][j] != m.weights[i][j] || 
                g_h[j] > 1e10f || g_h[j] < -1e10f || 
                m.weights[i][j] > 1e10f || m.weights[i][j] < -1e10f) {
                continue;  // Skip invalid values
            }
            g_c[i] += g_h[j]*m.weights[i][j];
        }
    }

    float *g_as=malloc(vs*sizeof(float)); 
    float scale=1.0f/sqrt(EMBEDDING_DIM);
    
    // Initialize attention score gradients
    for(int i=0;i<vs;i++) g_as[i]=0;
    
    for(int i=0;i<vs;i++){
        g_as[i]=0; 
        float nv_i[EMBEDDING_DIM]={v[i].embedding,v[i].pe,v[i].weight,v[i].bias1,v[i].bias2,v[i].bias3,v[i].bias4};
        for(int j=0;j<EMBEDDING_DIM;j++) {
            if (g_c[j] != g_c[j] || nv_i[j] != nv_i[j] || 
                g_c[j] > 1e10f || g_c[j] < -1e10f || 
                nv_i[j] > 1e10f || nv_i[j] < -1e10f) {
                continue;
            }
            g_as[i] += g_c[j]*nv_i[j]; 
        }
        // Apply scaling with safety check
        if (scale != scale || scale > 1e10f || scale < -1e10f) {
            scale = 1.0f;
        }
        g_as[i] *= scale;
        
        // Additional clipping to prevent large values
        if (g_as[i] > 10.0f) g_as[i] = 10.0f;
        if (g_as[i] < -10.0f) g_as[i] = -10.0f;
    }
    
    // Clip attention score gradients to prevent explosion
    clip_gradients(g_as, vs, 1.0f);
    
    // Add gradient noise for better generalization (small amount)
    add_gradient_noise(g_as, vs, 0.01f);

    // Backprop through softmax: forward computes as = softmax(asr).
    // g_raw[i] = p[i] * (g_as[i] - sum_k p[k]*g_as[k]) where p = softmax(asr).
    float asr_max = asr[0]; for(int i=1;i<vs;i++) if(asr[i]>asr_max) asr_max=asr[i];
    float asr_norm = 0.0f;
    for(int i=0;i<vs;i++) {
        if (asr[i] != asr[i] || asr[i] > 1e10f || asr[i] < -1e10f) { asr[i] = asr_max; }
    }
    float dot_pg = 0.0f;
    for(int i=0;i<vs;i++) { asr[i] = expf(asr[i]-asr_max); asr_norm += asr[i]; }
    if (asr_norm < 1e-12f || asr_norm != asr_norm) asr_norm = 1.0f;
    for(int i=0;i<vs;i++) { asr[i] /= asr_norm; dot_pg += asr[i]*g_as[i]; }
    for(int i=0;i<vs;i++) { g_as[i] = asr[i]*(g_as[i]-dot_pg); }
    
    float g_q[EMBEDDING_DIM]={0};
    float iv_wi[EMBEDDING_DIM]={v[wi].embedding,v[wi].pe,v[wi].weight,v[wi].bias1,v[wi].bias2,v[wi].bias3,v[wi].bias4};
    for(int i=0;i<vs;i++) {
        float nv_i[EMBEDDING_DIM]={v[i].embedding,v[i].pe,v[i].weight,v[i].bias1,v[i].bias2,v[i].bias3,v[i].bias4};
        for(int j=0;j<EMBEDDING_DIM;j++) {
            if (nv_i[j] != nv_i[j] || g_as[i] != g_as[i] || 
                nv_i[j] > 1e10f || nv_i[j] < -1e10f || 
                g_as[i] > 1e10f || g_as[i] < -1e10f) {
                continue;
            }
            g_q[j] += scale * nv_i[j] * g_as[i];
        }
    }
    
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<EMBEDDING_DIM;j++) {
            if (g_q[j] != g_q[j] || iv_wi[i] != iv_wi[i] || 
                g_q[j] > 1e10f || g_q[j] < -1e10f || 
                iv_wi[i] > 1e10f || iv_wi[i] < -1e10f) {
                g_a.W_q[i][j] = 0;
            } else {
                g_a.W_q[i][j] = g_q[j] * iv_wi[i];
            }
            // W_k and W_v do not feed the output path in forward_prop; zero grads
            g_a.W_k[i][j] = 0;
            g_a.W_v[i][j] = 0;
        }
    }
    
    // Clip attention layer gradients
    clip_gradients_2d((float*)&g_a, sizeof(g_a)/sizeof(float), 1, 1.0f);
    
    // Add gradient noise to attention gradients
    add_gradient_noise((float*)&g_a, sizeof(g_a)/sizeof(float), 0.005f);
    
    // Clip output layer gradients
    // Clip weights
    for(int i=0; i<HIDDEN_DIM; i++) {
        clip_gradients(g_o.weights[i], vs, 1.0f);
    }
    // Clip biases
    clip_gradients(g_o.biases, vs, 1.0f);
    
    // Log gradient norms for debugging
    float attn_norm = gradient_norm((float*)&g_a, sizeof(g_a)/sizeof(float));
    float mlp_norm = gradient_norm((float*)&g_m, sizeof(g_m)/sizeof(float));
    // Fix the output layer gradient norm calculation
    float output_norm = 0.0f;
    // Calculate norm for output layer weights
    for(int i=0; i<HIDDEN_DIM; i++) {
        for(int j=0; j<vs; j++) {
            if (g_o.weights[i][j] != g_o.weights[i][j] || g_o.weights[i][j] > 1e10f || g_o.weights[i][j] < -1e10f) {
                g_o.weights[i][j] = 0.0f;
            }
            output_norm += g_o.weights[i][j] * g_o.weights[i][j];
        }
    }
    // Calculate norm for output layer biases
    for(int i=0; i<vs; i++) {
        if (g_o.biases[i] != g_o.biases[i] || g_o.biases[i] > 1e10f || g_o.biases[i] < -1e10f) {
            g_o.biases[i] = 0.0f;
        }
        output_norm += g_o.biases[i] * g_o.biases[i];
    }
    output_norm = sqrtf(output_norm);
    fprintf(stderr, "Gradient norms - Attention: %f, MLP: %f, Output: %f\n", attn_norm, mlp_norm, output_norm);
    
    char pth[1024];
    sprintf(pth,"%s/grad_output.txt",od); save_grad_output(pth,&g_o,vs);
    sprintf(pth,"%s/grad_mlp.txt",od); save_grad_mlp(pth,&g_m);
    sprintf(pth,"%s/grad_attn.txt",od); save_matrix(pth,(float*)&g_a,sizeof(g_a)/sizeof(float),1);

    fprintf(stderr, "Backward propagation completed.\n");

    // Cleanup allocated memory
    free(gl);
    free(h);
    free(c);
    free(q);
    free(k);
    free(val);
    free(asr);
    free(g_h);
    free(g_c);
    free(g_iv);
    free(g_as);
    for(int i=0; i<HIDDEN_DIM; i++) {
        free(g_o.weights[i]);
        free(o.weights[i]);
    }
    free(g_o.weights);
    free(g_o.biases);
    free(o.weights);
    free(o.biases);
    free(v);

    return 0;
    return 0;
}