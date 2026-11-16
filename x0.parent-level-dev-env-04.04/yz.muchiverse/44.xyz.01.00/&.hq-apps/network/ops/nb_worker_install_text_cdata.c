/* nb_worker_install_text_cdata.c - Text/CharacterData prototype installs (mechanical extract, single-TU include) */
    {
        JSAtom nm = JS_NewAtom(ctx, "data");
        JS_DefinePropertyGetSet(ctx, Tp, nm,
            JS_NewCFunction(ctx, nb_text_data_get, "get data", 0),
            JS_NewCFunction(ctx, nb_text_data_set, "set data", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "length");
        JS_DefinePropertyGetSet(ctx, Tp, nm,
            JS_NewCFunction(ctx, nb_cdata_length_get, "get length", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, Tp, "substringData", JS_NewCFunction(ctx, nb_cdata_substringData, "substringData", 2));
    JS_SetPropertyStr(ctx, Tp, "appendData",    JS_NewCFunction(ctx, nb_cdata_appendData, "appendData", 1));
    JS_SetPropertyStr(ctx, Tp, "insertData",    JS_NewCFunction(ctx, nb_cdata_insertData, "insertData", 2));
    JS_SetPropertyStr(ctx, Tp, "deleteData",    JS_NewCFunction(ctx, nb_cdata_deleteData, "deleteData", 2));
    JS_SetPropertyStr(ctx, Tp, "replaceData",   JS_NewCFunction(ctx, nb_cdata_replaceData, "replaceData", 3));
    JS_SetPropertyStr(ctx, Tp, "splitText",     JS_NewCFunction(ctx, nb_text_splitText, "splitText", 1));
    {
        JSAtom nm = JS_NewAtom(ctx, "wholeText");
        JS_DefinePropertyGetSet(ctx, Tp, nm,
            JS_NewCFunction(ctx, nb_text_wholeText, "get wholeText", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
