#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgen.h>

#define EMBEDDING_DIM 7
#define HIDDEN_DIM 16
#define EPSILON 1e-8
#define MAX_GRAD_NORM 0.5f

// --- Data Structures ---
typedef struct { float weights[EMBEDDING_DIM][HIDDEN_DIM]; float biases[HIDDEN_DIM]; } MlpLayer;
typedef struct { float W_q[EMBEDDING_DIM][EMBEDDING_DIM], W_k[EMBEDDING_DIM][EMBEDDING_DIM], W_v[EMBEDDING_DIM][EMBEDDING_DIM]; } AttentionLayer;
typedef struct { float **weights; float *biases; } OutputLayer;

// --- File I/O ---
void load_attention(const char *fn, AttentionLayer *l) { FILE *f=fopen(fn,"r"); if(f){ for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f, "%f", &l->W_q[i][j]); for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f, "%f", &l->W_k[i][j]); for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fscanf(f, "%f", &l->W_v[i][j]); fclose(f); } }
void save_attention(const char *fn, AttentionLayer *l) { FILE *f=fopen(fn,"w"); if(f){ for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fprintf(f, "%.9g ", l->W_q[i][j]); fprintf(f, "\n"); for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fprintf(f, "%.9g ", l->W_k[i][j]); fprintf(f, "\n"); for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) fprintf(f, "%.9g ", l->W_v[i][j]); fprintf(f, "\n"); fclose(f); fprintf(stderr, "Saved attention to %s\n", fn); } }
void load_mlp(const char *fn, MlpLayer *l) { FILE *f=fopen(fn,"r"); if(f){ for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<HIDDEN_DIM;j++) fscanf(f, "%f", &l->weights[i][j]); for(int i=0;i<HIDDEN_DIM;i++) fscanf(f, "%f", &l->biases[i]); fclose(f); } }
void save_mlp(const char *fn, MlpLayer *l) { FILE *f=fopen(fn,"w"); if(f){ for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<HIDDEN_DIM;j++) fprintf(f, "%.9g ", l->weights[i][j]); fprintf(f, "\n"); for(int i=0;i<HIDDEN_DIM;i++) fprintf(f, "%.9g ", l->biases[i]); fprintf(f, "\n"); fclose(f); fprintf(stderr, "Saved MLP to %s\n", fn); } }

// Free memory allocated for OutputLayer
void free_output(OutputLayer *l, int vs) {
    if (l->weights != NULL) {
        for(int i = 0; i < HIDDEN_DIM; i++) {
            if (l->weights[i] != NULL) {
                free(l->weights[i]);
                l->weights[i] = NULL;
            }
        }
        free(l->weights);
        l->weights = NULL;
    }
    if (l->biases != NULL) {
        free(l->biases);
        l->biases = NULL;
    }
}

void load_output(const char *fn, OutputLayer *l, int vs) { 
    // Initialize pointers to NULL
    l->weights = NULL;
    l->biases = NULL;
    
    // Properly allocate 2D array for weights
    l->weights = malloc(HIDDEN_DIM * sizeof(float*));
    if (l->weights == NULL) {
        fprintf(stderr, "Memory allocation failed for output weights\n");
        exit(1);
    }
    for(int i = 0; i < HIDDEN_DIM; i++) {
        l->weights[i] = malloc(vs * sizeof(float));
        if (l->weights[i] == NULL) {
            fprintf(stderr, "Memory allocation failed for output weights[%d]\n", i);
            // Clean up previously allocated memory
            for(int j = 0; j < i; j++) {
                free(l->weights[j]);
            }
            free(l->weights);
            exit(1);
        }
    }
    
    l->biases = malloc(vs * sizeof(float));
    if (l->biases == NULL) {
        fprintf(stderr, "Memory allocation failed for output biases\n");
        // Clean up weights memory
        for(int i = 0; i < HIDDEN_DIM; i++) {
            free(l->weights[i]);
        }
        free(l->weights);
        exit(1);
    }
    
    FILE *f = fopen(fn, "r");
    if(f) {
        for(int i = 0; i < HIDDEN_DIM; i++) {
            for(int j = 0; j < vs; j++) {
                fscanf(f, "%f", &l->weights[i][j]);
            }
        }
        for(int i = 0; i < vs; i++) {
            fscanf(f, "%f", &l->biases[i]);
        }
        fclose(f);
    }
}

void save_output(const char *fn, OutputLayer *l, int vs) { 
    FILE *f = fopen(fn, "w");
    if(f) {
        for(int i = 0; i < HIDDEN_DIM; i++) {
            for(int j = 0; j < vs; j++) {
                fprintf(f, "%.9g ", l->weights[i][j]);
            }
            fprintf(f, "\n");
        }
        for(int i = 0; i < vs; i++) {
            fprintf(f, "%.9g ", l->biases[i]);
        }
        fprintf(f, "\n");
        fclose(f);
        fprintf(stderr, "Saved output layer to %s\n", fn);
    } else {
        fprintf(stderr, "Failed to open file %s for writing\n", fn);
    }
}

// --- Gradient Clipping ---
void clip_gradients(AttentionLayer *g_a, MlpLayer *g_m, OutputLayer *g_o, int vs) {
    float total_norm = 0;
    
    // Calculate norm with NaN/Inf checking
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<EMBEDDING_DIM;j++) {
            // Check for valid values
            if (g_a->W_q[i][j] != g_a->W_q[i][j] || g_a->W_q[i][j] > 1e10f || g_a->W_q[i][j] < -1e10f) {
                g_a->W_q[i][j] = 0.0f;  // Reset invalid values
            }
            total_norm += g_a->W_q[i][j] * g_a->W_q[i][j];
        }
    }
    
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<HIDDEN_DIM;j++) {
            // Check for valid values
            if (g_m->weights[i][j] != g_m->weights[i][j] || g_m->weights[i][j] > 1e10f || g_m->weights[i][j] < -1e10f) {
                g_m->weights[i][j] = 0.0f;  // Reset invalid values
            }
            total_norm += g_m->weights[i][j] * g_m->weights[i][j];
        }
    }
    
    for(int i=0;i<HIDDEN_DIM;i++) {
        // Check for valid values
        if (g_m->biases[i] != g_m->biases[i] || g_m->biases[i] > 1e10f || g_m->biases[i] < -1e10f) {
            g_m->biases[i] = 0.0f;  // Reset invalid values
        }
        total_norm += g_m->biases[i] * g_m->biases[i];
    }
    
    for(int i=0;i<HIDDEN_DIM;i++) {
        for(int j=0;j<vs;j++) {
            // Check for valid values
            if (g_o->weights[i][j] != g_o->weights[i][j] || g_o->weights[i][j] > 1e10f || g_o->weights[i][j] < -1e10f) {
                g_o->weights[i][j] = 0.0f;  // Reset invalid values
            }
            total_norm += g_o->weights[i][j] * g_o->weights[i][j];
        }
    }
    
    for(int i=0;i<vs;i++) {
        // Check for valid values
        if (g_o->biases[i] != g_o->biases[i] || g_o->biases[i] > 1e10f || g_o->biases[i] < -1e10f) {
            g_o->biases[i] = 0.0f;  // Reset invalid values
        }
        total_norm += g_o->biases[i] * g_o->biases[i];
    }
    
    // Prevent sqrt of negative numbers
    if (total_norm < 0) total_norm = 0;
    total_norm = sqrt(total_norm);
    
    // Normalize gradients per layer
    // Attention layer normalization
    float attn_norm = 0;
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<EMBEDDING_DIM;j++) {
            attn_norm += g_a->W_q[i][j] * g_a->W_q[i][j];
        }
    }
    attn_norm = sqrt(attn_norm);
    
    // MLP layer normalization
    float mlp_norm = 0;
    for(int i=0;i<EMBEDDING_DIM;i++) {
        for(int j=0;j<HIDDEN_DIM;j++) {
            mlp_norm += g_m->weights[i][j] * g_m->weights[i][j];
        }
    }
    for(int i=0;i<HIDDEN_DIM;i++) {
        mlp_norm += g_m->biases[i] * g_m->biases[i];
    }
    mlp_norm = sqrt(mlp_norm);
    
    // Output layer normalization
    float output_norm = 0;
    for(int i=0;i<HIDDEN_DIM;i++) {
        for(int j=0;j<vs;j++) {
            output_norm += g_o->weights[i][j] * g_o->weights[i][j];
        }
    }
    for(int i=0;i<vs;i++) {
        output_norm += g_o->biases[i] * g_o->biases[i];
    }
    output_norm = sqrt(output_norm);
    
    // Apply layer-wise normalization if norms are reasonable
    if (attn_norm > 1e-12f && attn_norm < 1e10f) {
        float attn_scale = 1.0f / attn_norm;
        for(int i=0;i<EMBEDDING_DIM;i++) {
            for(int j=0;j<EMBEDDING_DIM;j++) {
                g_a->W_q[i][j] *= attn_scale;
                // Final safety check
                if (g_a->W_q[i][j] != g_a->W_q[i][j] || g_a->W_q[i][j] > 1e10f || g_a->W_q[i][j] < -1e10f) {
                    g_a->W_q[i][j] = 0.0f;
                }
            }
        }
    }
    
    if (mlp_norm > 1e-12f && mlp_norm < 1e10f) {
        float mlp_scale = 1.0f / mlp_norm;
        for(int i=0;i<EMBEDDING_DIM;i++) {
            for(int j=0;j<HIDDEN_DIM;j++) {
                g_m->weights[i][j] *= mlp_scale;
                // Final safety check
                if (g_m->weights[i][j] != g_m->weights[i][j] || g_m->weights[i][j] > 1e10f || g_m->weights[i][j] < -1e10f) {
                    g_m->weights[i][j] = 0.0f;
                }
            }
        }
        for(int i=0;i<HIDDEN_DIM;i++) {
            g_m->biases[i] *= mlp_scale;
            // Final safety check
            if (g_m->biases[i] != g_m->biases[i] || g_m->biases[i] > 1e10f || g_m->biases[i] < -1e10f) {
                g_m->biases[i] = 0.0f;
            }
        }
    }
    
    if (output_norm > 1e-12f && output_norm < 1e10f) {
        float output_scale = 1.0f / output_norm;
        for(int i=0;i<HIDDEN_DIM;i++) {
            for(int j=0;j<vs;j++) {
                g_o->weights[i][j] *= output_scale;
                // Final safety check
                if (g_o->weights[i][j] != g_o->weights[i][j] || g_o->weights[i][j] > 1e10f || g_o->weights[i][j] < -1e10f) {
                    g_o->weights[i][j] = 0.0f;
                }
            }
        }
        for(int i=0;i<vs;i++) {
            g_o->biases[i] *= output_scale;
            // Final safety check
            if (g_o->biases[i] != g_o->biases[i] || g_o->biases[i] > 1e10f || g_o->biases[i] < -1e10f) {
                g_o->biases[i] = 0.0f;
            }
        }
    }
    
    // Global clipping if norm is too large
    if (total_norm > MAX_GRAD_NORM && total_norm > 1e-12f) {  // Add epsilon check
        float scale = MAX_GRAD_NORM / total_norm;
        // Ensure scale is reasonable
        if (scale != scale || scale > 1e10f || scale < -1e10f) {
            scale = 1.0f;
        }
        
        // Apply scaling with safety checks
        for(int i=0;i<EMBEDDING_DIM;i++) {
            for(int j=0;j<EMBEDDING_DIM;j++) {
                g_a->W_q[i][j] *= scale;
                // Final safety check
                if (g_a->W_q[i][j] != g_a->W_q[i][j] || g_a->W_q[i][j] > 1e10f || g_a->W_q[i][j] < -1e10f) {
                    g_a->W_q[i][j] = 0.0f;
                }
            }
        }
        
        for(int i=0;i<EMBEDDING_DIM;i++) {
            for(int j=0;j<HIDDEN_DIM;j++) {
                g_m->weights[i][j] *= scale;
                // Final safety check
                if (g_m->weights[i][j] != g_m->weights[i][j] || g_m->weights[i][j] > 1e10f || g_m->weights[i][j] < -1e10f) {
                    g_m->weights[i][j] = 0.0f;
                }
            }
        }
        
        for(int i=0;i<HIDDEN_DIM;i++) {
            g_m->biases[i] *= scale;
            // Final safety check
            if (g_m->biases[i] != g_m->biases[i] || g_m->biases[i] > 1e10f || g_m->biases[i] < -1e10f) {
                g_m->biases[i] = 0.0f;
            }
        }
        
        for(int i=0;i<HIDDEN_DIM;i++) {
            for(int j=0;j<vs;j++) {
                g_o->weights[i][j] *= scale;
                // Final safety check
                if (g_o->weights[i][j] != g_o->weights[i][j] || g_o->weights[i][j] > 1e10f || g_o->weights[i][j] < -1e10f) {
                    g_o->weights[i][j] = 0.0f;
                }
            }
        }
        
        for(int i=0;i<vs;i++) {
            g_o->biases[i] *= scale;
            // Final safety check
            if (g_o->biases[i] != g_o->biases[i] || g_o->biases[i] > 1e10f || g_o->biases[i] < -1e10f) {
                g_o->biases[i] = 0.0f;
            }
        }
    }
}

// --- Adam Update ---
void adam_update(float *p, float *m, float *v, float g, float lr, float b1, float b2, int t) { 
    // Check for valid gradient
    if (g != g || g > 1e10f || g < -1e10f) {
        g = 0.0f;  // Reset invalid gradient
    }
    
    // Update moment estimates with safety checks
    if (*m != *m || *m > 1e10f || *m < -1e10f) *m = 0.0f;
    if (*v != *v || *v > 1e10f || *v < -1e10f) *v = 0.0f;
    
    *m = b1 * *m + (1-b1) * g;
    *v = b2 * *v + (1-b2) * g * g;
    
    // Safety checks after updates
    if (*m != *m || *m > 1e10f || *m < -1e10f) *m = 0.0f;
    if (*v != *v || *v > 1e10f || *v < -1e10f) *v = 0.0f;
    
    // Calculate bias-corrected moments with safety checks
    float b1t = powf(b1, t);
    float b2t = powf(b2, t);
    
    // Prevent division by zero or invalid values
    float mh = 0.0f, vh = 0.0f;
    if (fabsf(1.0f - b1t) > 1e-12f) {
        mh = *m / (1.0f - b1t);
    }
    
    if (fabsf(1.0f - b2t) > 1e-12f) {
        vh = *v / (1.0f - b2t);
    }
    
    // Safety checks for bias-corrected moments
    if (mh != mh || mh > 1e10f || mh < -1e10f) mh = 0.0f;
    if (vh != vh || vh > 1e10f || vh < -1e10f) vh = 0.0f;
    
    // Update parameter with safety checks
    float denom = sqrtf(vh) + EPSILON;
    if (denom != denom || denom > 1e10f || denom < -1e10f) {
        denom = EPSILON;
    }
    
    float update = lr * mh / denom;
    // Safety check for update
    if (update != update || update > 1e10f || update < -1e10f) {
        update = 0.0f;
    }
    
    *p -= update;
    
    // Final safety check for parameter
    if (*p != *p || *p > 1e10f || *p < -1e10f) {
        *p = 0.0f;
    }
}

// --- Main ---
int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <operation> [args...]\n", argv[0]); return 1; }
    char *op = argv[1];

    if (strcmp(op, "adam-init") == 0) {
        if (argc < 5) { fprintf(stderr, "Usage: %s adam-init <state_file> <output_dir> <vocab_size>\n", argv[0]); return 1; }
        FILE *f=fopen(argv[2],"w"); if(f){ fprintf(f,"%.9g %.9g %.9g 0",0.0001,0.9,0.999); fclose(f); }
        char p[1024]; int vs=atoi(argv[4]);
        sprintf(p,"%s/attention_model.m.txt",argv[3]); f=fopen(p,"w"); for(int i=0;i<sizeof(AttentionLayer)/sizeof(float);i++) fprintf(f,"0.0 "); fclose(f);
        sprintf(p,"%s/attention_model.v.txt",argv[3]); f=fopen(p,"w"); for(int i=0;i<sizeof(AttentionLayer)/sizeof(float);i++) fprintf(f,"0.0 "); fclose(f);
        sprintf(p,"%s/mlp_model.m.txt",argv[3]); f=fopen(p,"w"); for(int i=0;i<sizeof(MlpLayer)/sizeof(float);i++) fprintf(f,"0.0 "); fclose(f);
        sprintf(p,"%s/mlp_model.v.txt",argv[3]); f=fopen(p,"w"); for(int i=0;i<sizeof(MlpLayer)/sizeof(float);i++) fprintf(f,"0.0 "); fclose(f);
        
        // Initialize output layer momentum files properly
        sprintf(p,"%s/output_layer.m.txt",argv[3]); 
        FILE *f_m = fopen(p,"w"); 
        if(f_m) {
            // Write zeros for weights (HIDDEN_DIM x vs matrix)
            for(int i = 0; i < HIDDEN_DIM; i++) {
                for(int j = 0; j < vs; j++) {
                    fprintf(f_m, "0.0 ");
                }
                fprintf(f_m, "\n");  // New line after each row
            }
            // Write zeros for biases (vs vector)
            for(int i = 0; i < vs; i++) {
                fprintf(f_m, "0.0 ");
            }
            fprintf(f_m, "\n");  // New line at the end
            fclose(f_m);
        }
        
        sprintf(p,"%s/output_layer.v.txt",argv[3]); 
        FILE *f_v = fopen(p,"w"); 
        if(f_v) {
            // Write zeros for weights (HIDDEN_DIM x vs matrix)
            for(int i = 0; i < HIDDEN_DIM; i++) {
                for(int j = 0; j < vs; j++) {
                    fprintf(f_v, "0.0 ");
                }
                fprintf(f_v, "\n");  // New line after each row
            }
            // Write zeros for biases (vs vector)
            for(int i = 0; i < vs; i++) {
                fprintf(f_v, "0.0 ");
            }
            fprintf(f_v, "\n");  // New line at the end
            fclose(f_v);
        }
        fprintf(stderr, "Adam optimizer initialized.\n");
    } else if (strcmp(op, "output-init") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: %s output-init <output_model> <vocab_size>\n", argv[0]); return 1; }
        int vs=atoi(argv[3]);
        OutputLayer o; o.weights=malloc(HIDDEN_DIM*sizeof(float*)); for(int i=0;i<HIDDEN_DIM;i++)o.weights[i]=malloc(vs*sizeof(float)); o.biases=malloc(vs*sizeof(float));
        for(int i=0;i<HIDDEN_DIM;i++) for(int j=0;j<vs;j++) o.weights[i][j]=((float)rand()/RAND_MAX-0.5f)*0.1f;
        for(int j=0;j<vs;j++) o.biases[j]=0.0f;
        save_output(argv[2], &o, vs);
        free_output(&o, vs);
        fprintf(stderr, "Output layer initialized in %s\n", argv[2]);
    } else if (strcmp(op, "update") == 0) {
        if (argc < 8) { fprintf(stderr, "Usage: %s update <vocab_size> <state_file> <attn_model> <mlp_model> <output_model> <grad_prefix>\n", argv[0]); return 1; }
        int vs=atoi(argv[2]); char *od=dirname(strdup(argv[4])); float lr,b1,b2; int t;
        FILE *sf=fopen(argv[3],"r"); if(sf){fscanf(sf,"%f %f %f %d",&lr,&b1,&b2,&t); fclose(sf);}
        t++;

        AttentionLayer attn, grad_attn, m_attn, v_attn;
        MlpLayer mlp, grad_mlp, m_mlp, v_mlp;
        OutputLayer output, grad_output, m_output, v_output;

        load_attention(argv[4], &attn); load_mlp(argv[5], &mlp); load_output(argv[6], &output, vs);
        char p[1024];
        sprintf(p,"%s_attn.txt",argv[7]); load_attention(p, &grad_attn);
        sprintf(p,"%s_mlp.txt",argv[7]); load_mlp(p, &grad_mlp);
        sprintf(p,"%s_output.txt",argv[7]); load_output(p, &grad_output, vs);
        sprintf(p,"%s/attention_model.m.txt",od); load_attention(p, &m_attn);
        sprintf(p,"%s/attention_model.v.txt",od); load_attention(p, &v_attn);
        sprintf(p,"%s/mlp_model.m.txt",od); load_mlp(p, &m_mlp);
        sprintf(p,"%s/mlp_model.v.txt",od); load_mlp(p, &v_mlp);
        sprintf(p,"%s/output_layer.m.txt",od); load_output(p, &m_output, vs);
        sprintf(p,"%s/output_layer.v.txt",od); load_output(p, &v_output, vs);

        clip_gradients(&grad_attn, &grad_mlp, &grad_output, vs);

        for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<EMBEDDING_DIM;j++) adam_update(&attn.W_q[i][j],&m_attn.W_q[i][j],&v_attn.W_q[i][j],grad_attn.W_q[i][j],lr,b1,b2,t);
        for(int i=0;i<EMBEDDING_DIM;i++) for(int j=0;j<HIDDEN_DIM;j++) adam_update(&mlp.weights[i][j],&m_mlp.weights[i][j],&v_mlp.weights[i][j],grad_mlp.weights[i][j],lr,b1,b2,t);
        for(int i=0;i<HIDDEN_DIM;i++) adam_update(&mlp.biases[i],&m_mlp.biases[i],&v_mlp.biases[i],grad_mlp.biases[i],lr,b1,b2,t);
        for(int i=0;i<HIDDEN_DIM;i++) for(int j=0;j<vs;j++) adam_update(&output.weights[i][j],&m_output.weights[i][j],&v_output.weights[i][j],grad_output.weights[i][j],lr,b1,b2,t);
        for(int i=0;i<vs;i++) adam_update(&output.biases[i],&m_output.biases[i],&v_output.biases[i],grad_output.biases[i],lr,b1,b2,t);

        save_attention(argv[4],&attn); save_mlp(argv[5],&mlp); save_output(argv[6],&output,vs);
        sprintf(p,"%s/attention_model.m.txt",od); save_attention(p,&m_attn);
        sprintf(p,"%s/attention_model.v.txt",od); save_attention(p,&v_attn);
        sprintf(p,"%s/mlp_model.m.txt",od); save_mlp(p,&m_mlp);
        sprintf(p,"%s/mlp_model.v.txt",od); save_mlp(p,&v_mlp);
        sprintf(p,"%s/output_layer.m.txt",od); save_output(p,&m_output,vs);
        sprintf(p,"%s/output_layer.v.txt",od); save_output(p,&v_output,vs);

        sf=fopen(argv[3],"w"); if(sf){fprintf(sf,"%.9g %.9g %.9g %d",lr,b1,b2,t); fclose(sf);}

        // Free dynamically allocated memory
        free_output(&grad_output, vs);
        free_output(&m_output, vs);
        free_output(&v_output, vs);
        free_output(&output, vs);

        fprintf(stderr, "Optimizer update completed.\n");
    } else { fprintf(stderr, "Unknown operation: %s\n", op); return 1; }
    return 0;
}