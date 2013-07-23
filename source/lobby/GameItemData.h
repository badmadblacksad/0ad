/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GAMEITEMDATA_H
#define GAMEITEMDATA_H

#include <string>

#define ITEMS \
	ITEM(name)      \
	ITEM(ip)        \
	ITEM(state)     \
	ITEM(nbp)       \
	ITEM(tnbp)      \
	ITEM(players)   \
	ITEM(mapName)   \
	ITEM(mapSize)   \
	ITEM(victoryCondition)

class GameItemData
{
	friend class XmppClient;
	friend class GameListQuery;
public:
	GameItemData() {}

	virtual ~GameItemData() {}

	gloox::Tag* tag() const
	{
		gloox::Tag* i = new gloox::Tag( "game" );

#define ITEM(param)\
	i->addAttribute( #param, m_##param );
		ITEMS
#undef ITEM

		return i;
	}

protected:
#define ITEM(param)\
	std::string m_##param ;
	ITEMS
#undef ITEM
};

#endif
