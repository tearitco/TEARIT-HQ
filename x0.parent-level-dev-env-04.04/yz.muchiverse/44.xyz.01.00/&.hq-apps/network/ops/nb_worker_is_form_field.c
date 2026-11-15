/* nb_worker_is_form_field.c - form-field tag helper (mechanical extract, single-TU include) */
/* rung-2 remainder: el.value for form fields — a get/set pair; the set
 * string is held on the wrapper (identity-cached per node) under a hidden
 * \xff key, and an unsets element falls back to its `value` attribute. */
static int is_form_field(const char *tag) {
    if (!tag) return 0;
    return !strcmp(tag, "input") || !strcmp(tag, "textarea")
        || !strcmp(tag, "select") || !strcmp(tag, "button")
        || !strcmp(tag, "option");
}
