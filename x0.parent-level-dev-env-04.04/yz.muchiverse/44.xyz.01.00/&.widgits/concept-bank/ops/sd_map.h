/* Bank weight is the number in words.pdl (0..1).
 * img_draft.py passes guidance_scale and num_inference_steps into
 * StableDiffusionPipeline. A weight of 0.50 is that script's default:
 * guidance 7.5, steps 20. Colour drives guidance. Detail (the crisp
 * word, or the part's style word) drives steps. */
static inline double sd_guidance(double colour_w) { return 7.5 * (0.5 + colour_w); }
static inline int sd_steps(double detail_w) { return (int)(20.0 * (0.5 + detail_w) + 0.5); }
