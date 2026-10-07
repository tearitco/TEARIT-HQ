/**
 * model_api.h — Unified LLM interface for XOD.
 * 
 * Reusable across: Ollama, OpenAI, Anthropic, local (llama.cpp), mock.
 * 
 * Usage:
 *   model_api_t *api = model_api_init("ollama", "http://10.0.0.144:11434", "gemma3:1b");
 *   model_response_t *resp = model_api_chat(api, system_prompt, user_prompt);
 *   // parse resp->content for action/reason/confidence
 *   model_response_free(resp);
 *   model_api_free(api);
 */

#ifndef XOD_MODEL_API_H
#define XOD_MODEL_API_H

#include <stddef.h>

typedef enum {
    MODEL_API_OLLAMA,
    MODEL_API_OPENAI,
    MODEL_API_ANTHROPIC,
    MODEL_API_LOCAL,
    MODEL_API_MOCK
} model_api_type_t;

typedef struct {
    char *content;          // raw response content
    char *model;            // model name
    double confidence;      // extracted or 0
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    int error_code;         // 0 = ok
    char *error_msg;
} model_response_t;

typedef struct model_api model_api_t;

model_api_t *model_api_init(model_api_type_t type, const char *base_url, const char *model_name, const char *api_key);

void model_api_free(model_api_t *api);

model_response_t *model_api_chat(model_api_t *api,
                                 const char *system_prompt,
                                 const char *user_prompt,
                                 double temperature,
                                 int max_tokens);

model_response_t *model_api_complete(model_api_t *api,
                                     const char *prompt,
                                     double temperature,
                                     int max_tokens);

void model_response_free(model_response_t *resp);

/* Utility: extract JSON action from response */
int model_extract_action(const char *content, char *action, size_t action_sz,
                         char *reason, size_t reason_sz, double *confidence);

#endif