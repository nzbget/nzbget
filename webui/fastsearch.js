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
 * $Revision: 778 $
 * $Date: 2013-08-07 22:09:43 +0200 (Mi, 07 Aug 2013) $
 *
 */

function FastSearcher()
{
	'use strict';

	this.source;
	this.len;
	this.p;

	this.initLexer = function(source)
	{
		this.source = source;
		this.len = source.length;
		this.p = 0;
	}
	
	this.nextToken = function()
	{
		while (this.p < this.len)
		{
			var ch = this.source[this.p++];
			switch (ch) {
				case ' ':
				case '\t':
					continue;

				case '-':
				case '(':
				case ')':
				case '|':
					return ch;

				default:
					this.p--;
					var token = '';
					var quote = false;
					while (this.p < this.len)
					{
						var ch = this.source[this.p++];
						if (quote)
						{
							if (ch === '"')
							{
								quote = false;
								ch = '';
							}
						}
						else
						{
							if (ch === '"')
							{
								quote = true;
								ch = '';
							}
							else if (' \t()|'.indexOf(ch) > -1)
							{
								this.p--;
								return token;
							}
						}
						token += ch;
					}
					return token;
			}
		}
		return null;
	}

	this.compile = function(searchstr)
	{
		var _this = this;
		this.initLexer(searchstr);

		function expression(greedy)
		{
			var node = null;
			while (true)
			{
				var token = _this.nextToken();
				switch (token)
				{
					case null:
					case ')':
						return node;

					case '-':
						node = _this.not(expression(false));
						break;

					case '(':
						node = expression(true);
						break;

					case '|':
						var node2 = expression(false); // suppress errors by trailing | (assume "true")
						node = node2 ? _this.or(node, node2) : node;
						break;

					default:
						var node2 = _this.term(token);
						node = node ? _this.and(node, node2) : node2;
				}

				if (!greedy && node)
				{
					return node;
				}
			}
		}

		this.root = expression(true);
	}

	this.root = null;
	this.text = null;
	this.error = false;

	this.exec = function(text) {
		if (this.error) {
			return null;
		}

		this.text = text;
		return this.root.eval();
	}

	this.and = function(L, R) {
		return {
			L: L, R: R,
			eval: function() { return L.eval() && R.eval(); }
		};
	}

	this.or = function(L, R) {
		return {
			L: L, R: R,
			eval: function() { return L.eval() || R.eval(); }
		};
	}

	this.not = function(M) {
		return {
			M: M,
			eval: function() { return !M.eval();}
		};
	}

	this.term = function(term) {
		var _this = this;
		return {
			term: term,
			eval: function() { return _this.find(term); }
		};
	}

	this.find = function(term) {
		return this.text.toLowerCase().indexOf(term) > -1;
	}
}
