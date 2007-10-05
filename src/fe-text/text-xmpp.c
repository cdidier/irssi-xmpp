#include "module.h"
#include "modules.h"

#include "text-xmpp-composing.h"

void
text_xmpp_init(void)
{
	text_xmpp_composing_init();

	module_register("xmpp", "text");
}

void
text_xmpp_deinit(void)
{
	 text_xmpp_composing_deinit();
}
