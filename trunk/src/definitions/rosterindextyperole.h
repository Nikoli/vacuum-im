#ifndef DEF_ROSTERINDEXTYPEROLE_H
#define DEF_ROSTERINDEXTYPEROLE_H

enum RosterIndexTypes {
	RIT_ANY_TYPE,
	RIT_ROOT,
	RIT_STREAM_ROOT,
	RIT_GROUP,
	RIT_GROUP_BLANK,
	RIT_GROUP_NOT_IN_ROSTER,
	RIT_GROUP_MY_RESOURCES,
	RIT_GROUP_AGENTS,
	RIT_CONTACT,
	RIT_AGENT,
	RIT_MY_RESOURCE
};

enum RosterIndexDataRoles {
	RDR_ANY_ROLE = 32,
	RDR_TYPE,
	RDR_INDEX_ID,
	//XMPP Roles
	RDR_STREAM_JID,
	RDR_JID,
	RDR_PJID,
	RDR_BARE_JID,
	RDR_NAME,
	RDR_GROUP,
	RDR_SHOW,
	RDR_STATUS,
	RDR_PRIORITY,
	RDR_SUBSCRIBTION,
	RDR_ASK,
	//Decoration Roles
	RDR_FONT_HINT,
	RDR_FONT_SIZE,
	RDR_FONT_WEIGHT,
	RDR_FONT_STYLE,
	RDR_FONT_UNDERLINE,
	//Labels roles
	RDR_LABEL_ID,
	RDR_LABEL_ORDERS,
	RDR_LABEL_VALUES,
	RDR_LABEL_FLAGS,
	RDR_FOOTER_TEXT,
	//Avatars
	RDR_AVATAR_HASH,
	RDR_AVATAR_IMAGE,
	//Annotations
	RDR_ANNOTATIONS,
	//IndexStates
	RDR_STATES_FORCE_ON,
	RDR_STATES_FORCE_OFF
};

#endif
