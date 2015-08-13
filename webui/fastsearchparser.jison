/*
 * This file is part of nzbget
 *
 * Copyright (C) 2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */

 /*
 * In this module:
 *   - Grammar defintion to create parser for using with fasttable component;
 *   - The grammar must be compiled into JavaScript with Jison -
 *     see <https://github.com/zaach/jison> for more details.
 *
 * Example search strings:
 *   game
 *   (game|king)
 *   (game|king) thrones
 *   "game of thrones"
 *   -"game of thrones"
 *   title:"game of thrones"
 *   -title:"game of thrones"
 *   (game|king) -thrones
 *   -(game|king) thrones
 *   status:downloading (game|king)
 *   (status:downloading|status:paused)
 *   -status:downloading -status:paused game
 *   par-status:success (unpack-status:none|unpack-status:success)
 *   par-status:success (unpack-status:none|unpack-status:success) game -thrones
 */

/* lexical grammar */
%lex
%%

\s+                   /* skip whitespace */
'"'("\\"["]|[^"])*'"'               return 'QUOTEDSTRING'
"'"('\\'[']|[^'])*"'"               return 'QUOTEDSTRING'
[A-Za-z]{1,}[A-Za-z_0-9\-]+\:      return 'FIELD'
[A-Za-z_0-9\.]+        return 'STRING'
"-"                   return '-'
"|"                   return '|'
"("                   return '('
")"                   return ')'
<<EOF>>               return 'EOF'
.                     return 'INVALID'

/lex

/* operator associations and precedence */

%left '|'
%left '-'
%left UMINUS

%start query

%% /* language grammar */

query
	: expressions EOF
		{
			yy.searcher.done($1);
		}
	;

expressions
	: e
		{
			$$ = $1;
		}
	| expressions e
		{
			$$ = yy.searcher.and($1, $2);
		}
	;

e
	: e '|' e
		{
			$$ = yy.searcher.or($1, $3);
		}
	| '-' e %prec UMINUS
		{
			$$ = yy.searcher.not($2);
		}
	| '(' e ')'
		{
			$$ = yy.searcher.braces($2);
		}
	| STRING
		{
			$$ = yy.searcher.term(null, $1);
		}
	| QUOTEDSTRING
		{
			$$ = yy.searcher.term(null, $1.substring(1, $1.length - 1));
		}
	| FIELD STRING
		{
			$$ = yy.searcher.term($1.substring(0, $1.length - 1), $2);
		}
	| FIELD QUOTEDSTRING
		{
			$$ = yy.searcher.term($1.substring(0, $1.length - 1), $2.substring(1, $2.length - 1));
		}
	;
