%{
#include "translator.tab.h"
%}

id "id"
name "name"
state "state"
desc "desc"
notes "notes"
edi "est-date-ini"
ede "est-date-end"
rdi "real-date-ini"
rde "real-date-end"
prior "priority"
type "type"
et "est-time"
time "time"

uname "uname"
urole "urole"

atid "atid"
auname "auname"

cadena [^\"\n]
intro [\n]

%%

{id}	    	{ return ID; }
{name}			{ return NAME; }
{state}			{ return STATE; }
{desc}			{ return DESCRIPTION; }
{notes}			{ return NOTES; }
{edi}			{ return EDATE_INI; }
{ede}			{ return EDATE_END; }
{rdi} 			{ return RDATE_INI; }
{rde} 			{ return RDATE_END; }
{prior}			{ return PRIORITY; }
{type}			{ return TYPE; }
{et}			{ return E_TIME; }
{time}			{ return R_TIME; }

{uname}			{ return UNAME; }
{urole}			{ return UROLE; }

{atid}			{ return ATID; }
{auname}		{ return AUNAME; }

{cadena}+		{ yylval.cad = strdup(yytext); return FIELD; }

{intro}			{ /* vacio */ }

.				{ /* vacio */ }

%%
