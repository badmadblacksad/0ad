var g_LobbyIsConnecting = false;
var g_InitialUsername = "";
var g_InitialPassword = "";

function init()
{
	g_InitialUsername = Engine.GetDefaultLobbyPlayerUsername();
	g_InitialPassword = Engine.GetDefaultLobbyPlayerPassword();
}

function lobbyStop()
{
	getGUIObjectByName("connectFeedback").caption = "";
	getGUIObjectByName("registerFeedback").caption = "";

	if (g_LobbyIsConnecting == false)
		return;

	g_LobbyIsConnecting = false;
	Engine.StopXmppClient();
}

function lobbyStart()
{
	if (g_LobbyIsConnecting != false)
		return;

	var username = getGUIObjectByName("connectUsername").caption;
	var password = getGUIObjectByName("connectPassword").caption;
	var playername = sanitisePlayerName(getGUIObjectByName("joinPlayerName").caption);
	var feedback = getGUIObjectByName("connectFeedback");

	if (!username || !password)
	{
		feedback.caption = "Username or password empty";
		return;
	}

	feedback.caption = "Connecting..";
	Engine.StartXmppClient(username, password, "arena", playername);
	g_LobbyIsConnecting=true;
	Engine.ConnectXmppClient();
}

function lobbyStartRegister()
{
	if (g_LobbyIsConnecting != false)
		return;

	var account = getGUIObjectByName("registerUsername").caption;
	var password = getGUIObjectByName("registerPassword").caption;
	var passwordAgain = getGUIObjectByName("registerPasswordAgain").caption;
	var feedback = getGUIObjectByName("registerFeedback");

	if (!account || !password || !passwordAgain)
	{
		feedback.caption = "Account name or password empty";
		return;
	}

	if (password != passwordAgain)
	{
		feedback.caption = "Password mismatch";
		getGUIObjectByName("registerPassword").caption = "";
		getGUIObjectByName("registerPasswordAgain").caption = "";
		return;
	}

	feedback.caption = "Registering...";
	Engine.StartRegisterXmppClient(account, password);
	g_LobbyIsConnecting = true;
	Engine.ConnectXmppClient();
}

function onTick()
{
	if (!g_LobbyIsConnecting)
		// The Xmpp Client has not been created
		return;

	// The XmppClient has been created, we are waiting
	// to be connected or to receive an error.

	//Wake up XmppClient
	Engine.RecvXmppClient();

	//Receive messages
	while (true)
	{
		var message = Engine.LobbyGuiPollMessage();
		if (!message)
			break;

		if (message.type == "muc" && message.level == "join")
		{
			// We are connected, switch to the lobby page
			Engine.PopGuiPage();
			var sname = sanitisePlayerName(getGUIObjectByName("joinPlayerName").caption);
			Engine.SwitchGuiPage("page_lobby.xml", { name: sname } );

			var username = getGUIObjectByName("connectUsername").caption;
			var password = getGUIObjectByName("connectPassword").caption;
			// Store latest player name
			Engine.SaveMPConfig(sname, Engine.GetDefaultMPServer());
			// Store latest username and password
			Engine.SetDefaultLobbyPlayerPair(username, password);

			return;
		}
		else if (message.type == "system" && message.text == "registered")
		{
			// Great, we are registered. Switch to the connection window.
			getGUIObjectByName("registerFeedback").caption = message.text;
			getGUIObjectByName("connectFeedback").caption = message.text;
			Engine.StopXmppClient();
			g_LobbyIsConnecting = false;
			getGUIObjectByName("connectUsername").caption = getGUIObjectByName("registerUsername").caption;
			getGUIObjectByName("connectPassword").caption = getGUIObjectByName("registerPassword").caption;
			getGUIObjectByName("pageRegister").hidden = true;
			getGUIObjectByName("pageConnect").hidden = false;
		}
		else if(message.type == "system" && (message.level == "error" || message.text == "disconnected"))
		{
			getGUIObjectByName("connectFeedback").caption = message.text;
			getGUIObjectByName("registerFeedback").caption = message.text;
			Engine.StopXmppClient();
			g_LobbyIsConnecting = false;
		}
	}
}

function sanitisePlayerName(name)
{
	// We delete the '[', ']' (GUI tags) and ',' (players names separator) characters
	// and limit the length to 20 characters
	return name.replace(/[\[\],]+/g,"").substr(0,20);
}
