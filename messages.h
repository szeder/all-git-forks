#ifndef MESSAGE_H
#define MESSAGE_H

#define MESSAGE_PUSH_NONFASTFORWARD 0
#define MESSAGE_STATUS_ADVICE 1

struct message_preference {
	const char *name;
	int preference;
};

extern struct message_preference messages[];

int git_default_message_config(const char *var, const char *value);

#endif /* MESSAGE_H */
