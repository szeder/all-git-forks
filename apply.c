#include "cache.h"
#include "apply.h"



static void git_apply_config(void)
{
	git_config_get_string_const("apply.whitespace", &apply_default_whitespace);
	git_config_get_string_const("apply.ignorewhitespace", &apply_default_ignorewhitespace);
	git_config(git_default_config, NULL);
}

int parse_whitespace_option(struct apply_state *state, const char *option)
{
	if (!option) {
		state->ws_error_action = warn_on_ws_error;
		return 0;
	}
	if (!strcmp(option, "warn")) {
		state->ws_error_action = warn_on_ws_error;
		return 0;
	}
	if (!strcmp(option, "nowarn")) {
		state->ws_error_action = nowarn_ws_error;
		return 0;
	}
	if (!strcmp(option, "error")) {
		state->ws_error_action = die_on_ws_error;
		return 0;
	}
	if (!strcmp(option, "error-all")) {
		state->ws_error_action = die_on_ws_error;
		state->squelch_whitespace_errors = 0;
		return 0;
	}
	if (!strcmp(option, "strip") || !strcmp(option, "fix")) {
		state->ws_error_action = correct_ws_error;
		return 0;
	}
	return error(_("unrecognized whitespace option '%s'"), option);
}

int parse_ignorewhitespace_option(struct apply_state *state,
				  const char *option)
{
	if (!option || !strcmp(option, "no") ||
	    !strcmp(option, "false") || !strcmp(option, "never") ||
	    !strcmp(option, "none")) {
		state->ws_ignore_action = ignore_ws_none;
		return 0;
	}
	if (!strcmp(option, "change")) {
		state->ws_ignore_action = ignore_ws_change;
		return 0;
	}
	return error(_("unrecognized whitespace ignore option '%s'"), option);
}

int init_apply_state(struct apply_state *state, const char *prefix)
{
	memset(state, 0, sizeof(*state));
	state->prefix = prefix;
	state->prefix_length = state->prefix ? strlen(state->prefix) : 0;
	state->apply = 1;
	state->line_termination = '\n';
	state->p_value = 1;
	state->p_context = UINT_MAX;
	state->squelch_whitespace_errors = 5;
	state->ws_error_action = warn_on_ws_error;
	state->ws_ignore_action = ignore_ws_none;
	state->linenr = 1;
	strbuf_init(&state->root, 0);

	git_apply_config();
	if (apply_default_whitespace && parse_whitespace_option(state, apply_default_whitespace))
		return -1;
	if (apply_default_ignorewhitespace && parse_ignorewhitespace_option(state, apply_default_ignorewhitespace))
		return -1;
	return 0;
}

