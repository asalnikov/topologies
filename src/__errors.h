/* The macro E() along with double-inclusion is used to ensure that ordering
 * of the strings remains synchronized. Idea taken from musl. */

E(TOP_E_FOPEN, "Could not open file")
E(TOP_E_FSTAT, "Could not stat file")
E(TOP_E_FMMAP, "Could not mmap file")
E(TOP_E_EVAL, "Could not evaluate")
E(TOP_E_CONN, "Could not connect")
E(TOP_E_NOMOD, "Could not find module")
E(TOP_E_NONET, "No network given")
E(TOP_E_BADGATE, "Gate is connected more than twice")
E(TOP_E_ALLOC, "Could not allocate memory")
E(TOP_E_JSON, "Invalid JSON")
E(TOP_E_TOKEN, "Bad token")
E(TOP_E_LOOP, "Bad loop boundaries")
E(TOP_E_REGEX, "Bad regex")

E(0, "No error information")
