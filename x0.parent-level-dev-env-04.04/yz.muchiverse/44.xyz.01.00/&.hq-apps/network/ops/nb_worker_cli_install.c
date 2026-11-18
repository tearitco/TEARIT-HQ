/* nb_worker_cli_install.c - CLI process/global install helper (mechanical extract, single-TU include) */
/* build a JS object from the process environment (not the full sys env —
 * see getenv below). Env exposure is opt-in via require('os')-free helper;
 * CLI-1 keeps it simple: expose a snapshot under process.env. */
static void nb_cli_install(JSContext *ctx, int argc, char **argv) {
    JSValue process = JS_NewObject(ctx);
    /* argv — node convention: [interpreter, script, args...] */
    JSValue argv_arr = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++) {
        JS_SetPropertyUint32(ctx, argv_arr, (uint32_t)i, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, process, "argv", argv_arr);

    /* env (snapshot of environ, ENAME="value" pairs) */
    JSValue env_obj = JS_NewObject(ctx);
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq) continue;
        char *k = strndup(*e, (size_t)(eq - *e));
        if (k) { JS_SetPropertyStr(ctx, env_obj, k, JS_NewString(ctx, eq + 1)); free(k); }
    }
    JS_SetPropertyStr(ctx, process, "env", env_obj);
    JS_SetPropertyStr(ctx, process, "cwd", JS_NewCFunction(ctx, nb_cli_cwd, "cwd", 0));
    /* stdout / stderr — each a small object with write() */
    JSValue so = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, so, "write", JS_NewCFunction(ctx, nb_cli_stdout, "write", 1));
    JS_SetPropertyStr(ctx, process, "stdout", so);
    JSValue se = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, se, "write", JS_NewCFunction(ctx, nb_cli_stderr, "write", 1));
    JS_SetPropertyStr(ctx, process, "stderr", se);
    JS_SetPropertyStr(ctx, process, "exit", JS_NewCFunction(ctx, nb_cli_exit, "exit", 1));

    /* expose as global `process` */
    JSValue pg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, pg, "process", process);
    JS_FreeValue(ctx, pg);
}
