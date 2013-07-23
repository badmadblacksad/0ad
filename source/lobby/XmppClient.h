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

#ifndef XXXMPPCLIENT_H
#define XXXMPPCLIENT_H

#if OS_WIN
// Prevent gloox from pulling in windows.h (which causes conflicts)
// and defines what is necessary in order to compile the lobby
#define _WINDOWS_

#ifndef OUT
#define OUT
#endif

	//Taken from WinDef.h
#define CONST const
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef ULONG *PULONG;
#define OPTIONAL

	// Taken from WinNT.h
#define VOID void
typedef char CHAR;
typedef long LONG;
typedef wchar_t WCHAR;
typedef __nullterminated WCHAR *NWPSTR, *LPWSTR, *PWSTR;
typedef __nullterminated CHAR *NPSTR, *LPSTR, *PSTR;
typedef __nullterminated CONST WCHAR *LPCWSTR, *PCWSTR;
typedef __nullterminated CONST CHAR *LPCSTR, *PCSTR;
typedef unsigned char       BYTE;
typedef BYTE  BOOLEAN;   
typedef BOOLEAN *PBOOLEAN;    
typedef void *PVOID;

	//Taken from BaseTsd.h
#if defined(_WIN64)
    typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
#else
    typedef _W64 unsigned long ULONG_PTR, *PULONG_PTR;
#endif
#include "specstrings.h"

#endif

//network
#include <gloox/jid.h>
#include <gloox/client.h>
#include <gloox/mucroom.h>
#include <gloox/message.h>

#include <gloox/messagehandler.h>
#include <gloox/presencehandler.h>
#include <gloox/mucroomhandler.h>
#include <gloox/loghandler.h>
#include <gloox/connectionlistener.h>
#include <gloox/registration.h>

#include <gloox/iq.h>
#include <gloox/iqhandler.h>

//game - script
#include <deque>
#include "scriptinterface/ScriptVal.h"

//Global Gamelist Extension
#define ExtGameListQuery 1403
const std::string XMLNS_GAMELIST = "jabber:iq:gamelist";

//Game - script
class ScriptInterface;
class GameItemData;

class XmppClient : public gloox::ConnectionListener, public gloox::MUCRoomHandler, public gloox::IqHandler, public gloox::LogHandler, public gloox::RegistrationHandler, public gloox::MessageHandler
{
private:
	//Game - script
	ScriptInterface& m_ScriptInterface;
	//Components
	gloox::Client* _client;
	gloox::MUCRoom* _mucRoom;
	gloox::Registration* _registration;
	//Account infos
	std::string _username;
	std::string _password;
	std::string _nick;
	std::string _xpartamuppId;

public:
	//Basic
	XmppClient(ScriptInterface& scriptInterface, std::string sUsername, std::string sPassword, std::string sRoom, std::string sNick, bool regOpt = false);
	virtual ~XmppClient();

	//Network
	void connect();
	void disconnect();
	void recv();
	void SendIqGetGameList();
	void SendIqRegisterGame(CScriptVal data);
	void SendIqUnregisterGame();
	void SendIqChangeStateGame(std::string nbp, std::string players);
	void SetNick(const std::string& nick);
	void kick(const std::string& nick, const std::string& reason);
	void ban(const std::string& nick, const std::string& reason);
	void SetPresence(const std::string& presence);
	void GetPresence(const std::string& nickname, std::string& presence);

	CScriptValRooted GUIGetPlayerList();
	CScriptValRooted GUIGetGameList();

	//Script
	ScriptInterface& GetScriptInterface();

protected:
	/* Xmpp handlers */
	/* MUC handlers */
	virtual void handleMUCParticipantPresence(gloox::MUCRoom*, const gloox::MUCRoomParticipant, const gloox::Presence&);
	virtual bool handleMUCRoomCreation(gloox::MUCRoom*) {return false;}
	virtual void handleMUCSubject(gloox::MUCRoom*, const std::string&, const std::string&) {}
	virtual void handleMUCInviteDecline(gloox::MUCRoom*, const gloox::JID&, const std::string&) {}
	virtual void handleMUCError(gloox::MUCRoom*, gloox::StanzaError);
	virtual void handleMUCInfo(gloox::MUCRoom*, int, const std::string&, const gloox::DataForm*) {}
	virtual void handleMUCItems(gloox::MUCRoom*, const std::list<gloox::Disco::Item*, std::allocator<gloox::Disco::Item*> >&) {}
	virtual void handleMUCMessage(gloox::MUCRoom* room, const gloox::Message& msg, bool priv);

	/* Log handler */
	virtual void handleLog(gloox::LogLevel level, gloox::LogArea area, const std::string& message);

	/* ConnectionListener handlers*/
	virtual void onConnect();
	virtual void onDisconnect(gloox::ConnectionError e);
	virtual bool onTLSConnect(const gloox::CertInfo& info);

	/* Iq Handlers */
	virtual bool handleIq(const gloox::IQ& iq);
	virtual void handleIqID(const gloox::IQ&, int) {}

	/* Registration Handlers */
	virtual void handleRegistrationFields(const gloox::JID& /*from*/, int fields, std::string instructions );
	virtual void handleRegistrationResult(const gloox::JID& /*from*/, gloox::RegistrationResult result);
	virtual void handleAlreadyRegistered(const gloox::JID& /*from*/);
	virtual void handleDataForm(const gloox::JID& /*from*/, const gloox::DataForm& /*form*/);
	virtual void handleOOB(const gloox::JID& /*from*/, const gloox::OOB& oob);

	/* Message Handler */
	virtual void handleMessage(const gloox::Message& msg, gloox::MessageSession * session);

	// Helper
	void GetPresenceString(const gloox::Presence::PresenceType p, std::string& presence) const;
public:
	/* Messages */
	CScriptValRooted GuiPollMessage();
	void SendMUCMessage(std::string message);
	protected:
	void PushGuiMessage(const CScriptValRooted& message);
	void CreateSimpleMessage(std::string type, std::string text, std::string level = "standard", std::string data = "");

private:
	/// Map of players
	std::map<std::string, gloox::Presence::PresenceType> m_PlayerMap;
	/// List of games
	std::list< GameItemData > m_GameList;
	/// Queue of messages
	std::deque<CScriptValRooted> m_GuiMessageQueue;
};

class GameListQuery : public gloox::StanzaExtension
{
	friend class XmppClient;
public:
	GameListQuery(const gloox::Tag* tag = 0);

	~GameListQuery();

	// reimplemented from StanzaExtension
	virtual const std::string& filterString() const;

	// reimplemented from StanzaExtension
	virtual StanzaExtension* newInstance(const gloox::Tag* tag) const
	{
		return new GameListQuery( tag );
	}

	// reimplemented from StanzaExtension
	virtual gloox::Tag* tag() const;

	// reimplemented from StanzaExtension
	virtual gloox::StanzaExtension* clone() const;

	const std::list<GameItemData*>& gameList() const;

private:
	std::string m_command;
	std::list<GameItemData*> m_gameList;
};

extern XmppClient *g_XmppClient;

#endif // XMPPCLIENT_H
