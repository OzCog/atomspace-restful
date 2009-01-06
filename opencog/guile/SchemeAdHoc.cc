/*
 * SchemeAdHoc.c
 *
 * Scheme adhoc callback into opencog
 *
 * Copyright (c) 2008 Linas Vepstas <linas@linas.org>
 */

#ifdef HAVE_GUILE

#include <libguile.h>

#include <opencog/server/CogServer.h>
#include <opencog/nlp/wsd/WordSenseProcessor.h>
#include <opencog/query/PatternMatch.h>

#include "SchemeSmob.h"

using namespace opencog;

/* ============================================================== */

/**
 * Dispatcher to invoke various miscellaneous C++ riff-raff from
 * scheme code. 
 */
SCM SchemeSmob::ss_ad_hoc(SCM command, SCM optargs)
{
	std::string cmdname = decode_string (command, "cog-ad-hoc");

	if (0 == cmdname.compare("do-wsd"))
	{
		WordSenseProcessor wsp;
		wsp.use_threads(false);
		wsp.run_no_delay(static_cast<CogServer *>(&server())); // XXX What an ugly interface. Alas.
		return SCM_BOOL_T;
	}

	// Run implication, assuming that the argument is a handle to 
	// an ImplicationLink
	if (0 == cmdname.compare("do-implication"))
	{
		// XXX we should also allow opt-args to be a list of handles
		Handle h = verify_handle(optargs, "cog-ad-hoc do-implication");

		CogServer *s = static_cast<CogServer *>(&server());
		AtomSpace *as = s->getAtomSpace();

		PatternMatch pm;
		pm.set_atomspace(as);
		Handle grounded_expressions = pm.imply(h);
		return handle_to_scm(grounded_expressions);
	}
	return SCM_BOOL_F;
}

#endif
/* ===================== END OF FILE ============================ */
