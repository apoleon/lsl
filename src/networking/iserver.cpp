#include "iserver.h"

#include "socket.h"


namespace LSL {

iServer::iServer()
    : m_keepalive(15),
    m_ping_timeout(40),
    m_ping_interval(10),
    m_server_rate_limit(800),
    m_message_size_limit(1024),
    m_connected(false),
    m_online(false),
    m_me(0),
    //m_last_udp_ping(0),
    //m_last_net_packet(0),
    m_udp_private_port(0),
    m_udp_reply_timeout(0),
    m_current_battle(0),
    m_buffer(""),
    m_relay_host_bot(0)
{
}

void iServer::Connect( const std::string& servername ,const std::string& addr, const int port )
{
	m_buffer = "";
	m_sock->Connect( addr, port );
    m_sock->SetSendRateLimit( m_server_rate_limit );
	m_connected = false;
    m_online = false;
    m_redirecting = false;
    m_agreement = "";
	m_crc.ResetCRC();
	m_last_net_packet = 0;
	std::string handle = m_sock->GetHandle();
	if ( handle.lenght() > 0 ) m_crc.UpdateData( handle + m_addr );
}

void iServer::Disconnect(const std::string& reason)
{
    if (!m_connected)
    {
        return;
    }
	_Disconnect(reason);
	m_sock->Disconnect();
}

bool ::IsOnline() const
{
	if ( !m_connected ) return false;
	return m_online;
}

bool iServer::IsConnected()
{
	return (m_sock->State() == SS_Open);
}



void iServer::TimerUpdate()
{

	if ( !IsConnected() ) return;

	time_t now = time( 0 );

	if ( ( m_last_net_packet > 0 ) && ( ( now - m_last_net_packet ) > m_ping_timeout ) )
	{
		m_se->OnTimeout();
		Disconnect();
		return;
	}

	// joining battle with nat traversal:
	// if we havent finalized joining yet, and udp_reply_timeout seconds has passed since
	// we did UdpPing(our name) , join battle anyway, but with warning message that nat failed.
	// (if we'd receive reply from server, we'd finalize already)
	//
	if (m_last_udp_ping > 0 && (now - m_last_udp_ping) > m_udp_reply_timeout )
	{
		m_se->OnNATPunchFailed();
	};

	// Is it time for a nat traversal PING?
	if ( ( m_last_udp_ping + m_keepalive ) < now )
	{
		m_last_udp_ping = now;
		// Nat travelsal "ping"
		if ( m_current_battle_id > 0 )
		{
			Battle *battle=GetCurrentBattle();
			if (battle && !battle->GetInGame() )
			{
				if ( battle->GetNatType() == NAT_Hole_punching || battle->GetNatType() == NAT_Fixed_source_ports  )
				{
					UdpPingTheServer();
					if ( battle->IsFounderMe() )
					{
						UdpPingAllClients();
					}
					}
				}
			}
		}
	}

}

void iServer::OnDataReceived( Socket* sock )
{
	std::string data = sock->Receive();
	m_buffer += data;
	int returnpos = m_buffer.find( "\n" );
	while ( returnpos != -1 )
	{
		std::string cmd = m_buffer.Left( returnpos );
		m_buffer = m_buffer.Mid( returnpos + 1 );
		ExecuteCommand( cmd );
		returnpos = m_buffer.Find( "\n" );
	}
}

void iServer::Ping()
{
	_Ping();
	GetPingList()[GetLastPingID()] = time(0);
}

void iServer::HandlePong( int replyid )
{
	PingList& pinglistcopy = GetPingList();
	PingList::iterator itor = pinglistcopy->find(replyid);
	if ( itor != pinglistcopy->end() )
	{
        m_se->OnPong( (time(0) - itor->second()) );
		pinglistcopy.erase( itor );
    }
}

void iServer::JoinChannel( const std::string& channel, const std::string& key )
{
    m_channel_pw[channel] = key;
	_JoinChannel(channel,key);
}

User* iServer::AcquireRelayhost()
{
	UserVector relaylist = GetRelayHostList();
	unsigned int numbots = relaylist.size();
	if ( numbots > 0 )
	{
		srand ( time(NULL) );
		unsigned int choice = rand() % numbots;
		m_relay_host_manager = relaylist[choice];
		SayPrivate( m_relay_host_manager, "!spawn" );
   }
}

void iServer::OpenBattle( BattleOptions bo )
{
	if ( bo.userelayhost )
	{
		AcquireRelayhost();
		m_last_relay_host_password = bo.password;
	}

	if ( bo.nattype > 0 ) UdpPingTheServer();
	_HostBattle(bo);
}

std::string GenerateScriptPassword()
{
	char buff[8];
	sprintf(&buff,"%04x%04x", rand()&0xFFFF, rand()&0xFFFF);
	return std::string(buff);
}

void iServer::JoinBattle( Battle* battle, const std::string& password )
{
	if (battle)
	{
	if ( battle->GetNatType() == NAT_Hole_punching || battle->GetNatType() == NAT_Fixed_source_ports )
	{
		for (int n=0;n<5;++n) // do 5 udp pings with tiny interval
		{
			UdpPingTheServer( m_user );
			m_last_udp_ping = time(0);
		}
	}
	srand ( time(NULL) );

	_JoinBattle(battle,password,GenerateScriptPassword());
}


void iServer::StartHostedBattle()
{
	if (!m_current_battle) return;
	if (!m_current_battle->IsFounderMe()) return;
	if ( m_current_battle->GetNatType() == NAT_Hole_punching || m_current_battle->GetNatType() == NAT_Fixed_source_ports )
	{
		UdpPingTheServer();
		for (int i=0;i<5;++i)
		{
			UdpPingAllClients();
		}
	}
	_StartHostedBattle();
	m_se->OnStartHostedBattle( m_battle_id );
}

void iServer::LeaveBattle( const Battle* battle)
{
    m_relay_host_bot = 0;
    _LeaveBattle(battle);
}

void iServer::BattleKickPlayer( const Battle* battle, const User* user )
{
	if (!battle) return;
	if (!battle.IsFounderMe()) return;
	if (!battle.IsProxy()) return;
	if (!user) return;
	user->BattleStatus().scriptPassword = GenerateScriptPassword(); // reset user script password, so he can't rejoin
	SetRelayIngamePassword( user );
}


UserVector iServer::GetRelayHostList()
{
	if ( m_relay_host_list )
	{
		SayPrivate( m_relay_host_list, "!listmanagers" );
	}
	UserVector ret;
	for ( unsigned int i = 0; i < m_relay_host_manager_list.count(); i++ )
	{
		User* manager = GetUser( m_relay_host_manager_list[i] );
		// skip the manager is not connected or reports it's ingame ( no slots available ), or it's away ( functionality disabled )
		if (!manager) continue;
		if ( manager->Status().in_game ) continue;
		if ( manager->Status().away ) continue;
		ret.push_back( manager );
	}
	return ret;
}

void iServer::RelayCmd(  const std::string& command, const std::string& param )
{
	if ( m_relay_host_bot )
	{
		SayPrivate( m_relay_host_bot, "!" + command + " " + param );
	}
	else
	{
		SendCmd( command, param );
	}
}

void iServer::SetRelayIngamePassword( const User* user )
{
	if (!user) return;
	if (!m_current_battle) return;
	if ( !m_current_battle->GetInGame() ) return;
	RelayCmd( "SETINGAMEPASSWORD", user->GetNick() + " " + user->BattleStatus().scriptPassword );
}

int iServer::RelayScriptSendETA(const std::string& script)
{
	StringVector strings = StringTokenize( script, _T("\n") );
	int relaylenghtprefix = 10 + 1 + m_relay_host_bot.GetNick().lenght() + 2; // SAYPRIVATE + space + botname + space + exclamation mark lenght
	int lenght = script.lenght();
	lenght += relaylenghtprefix + 11 + 1; // CLEANSCRIPT command size
	lenght += strings.count() * ( relaylenghtprefix + 16 + 1 ); // num lines * APPENDSCRIPTLINE + space command size ( \n is already counted in script.size)
	lenght += relaylenghtprefix + 9 + 1; // STARTGAME command size
	return lenght / m_sock->GetSendRateLimit(); // calculate time in seconds to upload script
}

void iServer::SendScriptToProxy( const std::string& script )
{
	StringVector strings = StringTokenize( script, _T("\n") );
	RelayCmd( "CLEANSCRIPT" );
	for (StringVector::iterator itor; itor != strings->end(); itor++)
	{
		RelayCmd( "APPENDSCRIPTLINE", *itor );
	}
	RelayCmd( "STARTGAME" );
}


//! @brief Send udp ping.
//! @note used for nat travelsal.

unsigned int iServer::UdpPing(unsigned int src_port, const std::string &target, unsigned int target_port, const std::string &message)// full parameters version, used to ping all clients when hosting.
{
	return result;
}

void iServer::UdpPingTheServer(const std::string &message)
{
	unsigned int port = UdpPing( m_udp_private_port, m_addr, m_nat_helper_port,message);
	if ( port>0 )
	{
		m_udp_private_port = port;
		m_se->OnMyInternalUdpSourcePort( m_udp_private_port );
	}
}


// copypasta from spring.cpp , to get users ordered same way as in tasclient.
struct UserOrder
{
	int index;// user number for GetUser
	int order;// user order (we'll sort by it)
	bool operator<(UserOrder b) const  // comparison function for sorting
	{
		return order<b.order;
	}
};


void TASServer::UdpPingAllClients()// used when hosting with nat holepunching. has some rudimentary support for fixed source ports.
{
	if (!m_current_battle)return;
	if (!m_current_battle->IsFounderMe())return;

	// I'm gonna mimic tasclient's behavior.
	// It of course doesnt matter in which order pings are sent,
	// but when doing "fixed source ports", the port must be
	// FIRST_UDP_SOURCEPORT + index of user excluding myself
	// so users must be reindexed in same way as in tasclient
	// to get same source ports for pings.


	// copypasta from spring.cpp
	UserVector ordered_users = m_current_battle->GetUsers();
	std::sort(ordered_users.begin(),ordered_users.end());


	for (UserVector::iterator itor = ordered_users.begin(); itor != ordered_users.end(); itor++)
	{
		if (!*itor) continue;
		UserBattleStatus status = *itor->BattleStatus();
		std::string ip=status.ip;
		unsigned int port=status.udpport;
		unsigned int src_port = m_udp_private_port;
		if ( battle->GetNatType() == NAT_Fixed_source_ports )
		{
			port = FIRST_UDP_SOURCEPORT + i;
		}

		if (port != 0 && ip.lenght() )
		{
			UdpPing(src_port, ip, port, "hai!" );
		}
	}
}




////////////////////////////////////////////////////////////////////////////////
/////                       Internal Server Events                         /////
////////////////////////////////////////////////////////////////////////////////


void iServer::OnSocketConnected(Socket* sock)
{
	m_connected = true;
	m_online = false;
	m_last_udp_ping = 0;
    m_min_required_spring_ver = "";
	m_relay_host_manager_list.clear();
	GetLastPingID() = 0;
	GetPingList().clear();
}

void iServer::OnDisconnected(Socket* sock)
{
    bool connectionwaspresent = m_online || !m_last_denied.lenght() || m_redirecting;
	m_connected = false;
	m_online = false;
    m_redirecting = false;
    m_last_denied = "";
    m_min_required_spring_ver = "";
	m_relay_host_manager_list.clear();
	GetLastPingID() = 0;
	GetPingList().clear();
	// delete all users, battles, channels
	m_se->OnDisconnected( connectionwaspresent );
}


void iServer::OnDataReceived( Socket* sock )
{
	if ( !sock ) return;
	m_last_net_packet = time( 0 );
	_OnDataRecieved(sock);
}


void iServer::OnSocketError( const Sockerror& /*unused*/ )
{
}


void iServer::OnProtocolError( const Protocolerror /*unused*/ )
{
}

////////////////////////////////////////////////////////////////////////////////
/////                        Command Server Events                         /////
////////////////////////////////////////////////////////////////////////////////


void iServer::OnServerInitialData(const std::string& server_name, const std::string& server_ver, bool supported, const std::string& server_spring_ver, bool /*unused*/)
{
	m_server_name = server_name;
	m_server_ver = server_ver;
    m_min_required_spring_ver = server_spring_ver;
}



void iServer::OnNewUser( const User* user )
{
	if (user.GetNick() == "RelayHostManagerList" )
	{
		m_relay_host_manager_list = user;
		SayPrivate( user, "!lm" );
	}
	if (user.GetNick() == m_relay_host_bot_nick )
	{
		m_relay_host_bot = user;
	}
}

void iServer::OnUserStatus( const User* user, UserStatus status )
{
	if ( !User ) return;
	UserStatus oldStatus = user->GetStatus();
	user->SetStatus( status );

	//TODO: event
}

void iServer::OnBattleStarted( const Battle* battle )
{
	if (!battle) return;
	//TODO: event
}


void iServer::OnDisconnected( bool wasonline )
{
	//TODO: event
}

void iServer::OnLogin( const User* user )
{
	if (m_online) return;
	m_online = true;
	m_me = user;
	m_ping_thread = new PingThread( *this, m_ping_interval*1000 );
	m_ping_thread->Init();
	//TODO: event
}

void iServer::OnLogout()
{
	//TODO: event
}

void iServer::OnLoginInfoComplete()
{
	//TODO: event
}

void iServer::OnUnknownCommand( const std::string& command, const std::string& params )
{
	//TODO: log
	//TODO: event
}


void iServer::OnMotd( const std::string& msg )
{
	//TODO: event
}


void iServer::OnPong( long long ping_time )
{
	//TODO: event
}



void iServer::OnUserQuit( const User* user )
{
	if ( !user ) return;
	if (user == m_me) return;
	RemoveUser( user );
	//TODO: event
}


void iServer::OnBattleOpened( Battle* battle )
{

	if ( battle.GetFounder() == m_relay_host_bot )
	{
		battle->SetProxy( m_relay_host_bot );
		JoinBattle( battle, m_last_relay_host_password ); // autojoin relayed host battles
	}
}

void iServer::OnBattleMapChanged(const Battle* battle,UnitsyncMap map)
{
	if (!battle) return;
	battle->SetHostMap( map.name, map.hash );
}

void iServer::OnBattleModChanged( const Battle* battle, UnitsyncMod mod )
{
	if (!battle) return;
	battle->SetHostMod( mod.name, mod.hash );
}

void iServer::OnBattleMaxPlayersChanged( const Battle* battle, int maxplayers )
{
	if (!battle) return;
	battle->SetMaxPlayers( maxplayers );
}

void iServer::OnBattleHostChanged( const Battle* battle, User* host, const std::string& ip, int port )
{
	if (!battle) return;
	if (!user) battle->SetFounder( host );
	battle->SetHostIp( ip );
	battle->SetHostPort( port );
}

void iServer::OnBattleSpectatorCountUpdated(const Battle* battle,int spectators)
{
	if (!battle) return;
	battle->SetNumSpectators(spectators);
}

void iServer::OnUserJoinedBattle( const Battle* battle, const User* user )
{
	if (!battle) return;
	if (!user) return;
	if (battle->IsProxy()) RelayCmd("SUPPORTSCRIPTPASSWORD"); // send flag to relayhost marking we support script passwords
}

void iServer::OnAcceptAgreement( const std::string& agreement )
{

}


void iServer::OnRing( const User* from )
{

}

void iServer::OnServerBroadcast( const std::string& message )
{

}

void iServer::OnServerMessage( const std::string& message )
{

}


void iServer::OnServerMessageBox( const std::string& message )
{
}


void iServer::OnChannelMessage( const Channel* channel, const std::string& msg )
{
	if (!channel) return;
}

void iServer::OnBattleLockUpdated(const Battle* battle,bool locked)
{
	if (!battle) return;
	battle->SetLocked(locked);
}

void iServer::OnUserLeftBattle(const Battle* battle, const User* user)
{
	if (!user) return;
	bool isbot = user->BattleStatus().IsBot();
	user->BattleStatus().scriptPassword.Clear();
	if (!battle) return;
	battle->OnUserRemoved( user );
	if (user == m_me)
	{
		m_relay_host_bot = 0;
	}
	//TODO: event
}

void iServer::OnBattleClosed(const Battle* battle )
{
	RemoveBattle( battleid );
	//TODO:event
}

void iServer::OnBattleDisableUnit( const Battle* battle, const std::string& unitname, int count )
{
	if (!battle) return;
	battle->RestrictUnit( unitname, count );
	//TODO: event
}

void iServer::OnBattleEnableUnit( int battleid, const StringVector& unitnames )
{
	if (!battle) return;
	for (StringVector::iterator itor = unitnames.begin(); itor!= unitnames.end(); itor++)
	{
		battle->UnrestrictUnit( *itor );
	}
	//TODO: event
}

void iServer::OnBattleEnableAllUnits( int battleid )
{
	if (!battle) return;
	battle->UnrestrictAllUnits();
	//TODO: event
}


void iServer::OnJoinChannelFailed( const Channel* channel, const std::string& reason )
{
	if (!channel) return;
	//TODO: event
}

void iServer::OnUserJoinedChannel( const Channel* channel, const User* user )
{
	if (!channel) return;
	if (!user) return;
	channel->OnChannelJoin( user );
	//TODO: event
}

void iServer::OnKickedFromChannel( const Channel* channel, const std::string& fromWho, const std::string& msg )
{
	if (!channel) return;
}

void iServer::OnLoginFailed( const std::string& reason )
{
	//TODO: check if disconnect can be removed
	Disconnect();
}

void iServer::OnChannelSaid( const Channel* channel, const User* user, const std::string& message )
{
	if ( m_relay_host_bot != 0 && channel == GetChannel( "U" + ToString(m_relay_host_bot->GetID()) )
	{
		if ( user == m_me && message.lenght() > 0 && message[0] == '!') ) return;
		if ( user == m_relay_host_bot )
		{
			if ( message.StartsWith("UserScriptPassword")) )
			{
				GetWordParam( message ); // skip the command keyword
				std::string usernick = GetWordParam( message );
				std::string userScriptPassword = GetWordParam( message );
				User* usr = GetUser(usernick);
				if (!usr) return;
				OnUserScriptPassword(user, userScriptPassword);
				return;
			}
		}
	}
	if ( m_relay_host_manager != 0 && channel == GetChannel( "U" + ToString(m_relay_host_manager->GetID()) )
	{
		if ( user == m_me &&  message.lenght() > 0 && message[0] == '!') ) return;
		if ( user == m_relay_host_manager )
		{
		if ( message.lenght() > 0 && message[0] == '\001' ) // error code
		{
		}
		else
		{
			m_relay_host_bot_nick = message;
			return;
		}
		}
	}
	if ( m_relay_host_manager_list != 0 && channel == GetChannel( "U" + ToString(m_relay_host_manager_list->GetID()) )
	{
		if ( user == m_me && message == "!lm" ) return;
		if ( user == m_relay_host_manager_list )
		{
			if  ( message.StartsWith("list ")) )
			{
				 std::string list = params.AfterFirst( ' ') );
				 m_relay_host_manager_list = std::stringTokenize( list, "\t") );
				 return;
			}
		}
	}
}

void iServer::OnBattleStartRectAdd( const Battle* battle, int allyno, int left, int top, int right, int bottom )
{
	if(!battle) return;
	battle->AddStartRect( allyno, left, top, right, bottom );
	battle->StartRectAdded( allyno );
}

void iServer::OnBattleStartRectRemove( const Battle* battle, int allyno )
{
	if (!battle) return;
	battle->RemoveStartRect( allyno );
	battle->StartRectRemoved( allyno );
}

void iServer::OnFileDownload( bool autolaunch, bool autoclose, bool /*disconnectonrefuse*/, const std::string& FileName, const std::string& url, const std::string& description )
{
	UTASOfferFileData parsingdata;
	parsingdata.data = GetIntParam( params );
}

void iServer::OnBattleScript( const Battle* battle, const std::string& script )
{
	if (!battle) return;
	battle->GetBattleFromScript( true );
}

void iServer::OnMuteList(const Channel* channel, const MuteList& mutelist )
{
}

void iServer::OnKickedFromBattle( const Battle* battle)
{
	if (!battle) return;
}


void iServer::OnUserInternalUdpPort( const User* user, int udpport )
{
	if (!user) return;
}

void iServer::OnUserExternalUdpPort( const User* user, int udpport )
{
	if (!user) return;
	user->BattleStatus().extudpport = udpport;
}

void iServer::OnUserIP( const User* user, const std::string& ip )
{
	if (!user) return;
	user->BattleStatus().ip = ip;
}

void iServer::OnRedirect( const std::string& address, int port )
{
	if (!address.lenght()) return;
	if (!port) return;
}

void iServer::OnChannelJoinUserList( const Channel* channel, const UserVector& users)
{

}

void TASServer::OnChannelListEnd()
{
}

void iServer::OnJoinBattleFailed( const std::string& msg )
{
}


void iServer::OnOpenBattleFailed( const std::string& msg )
{
}

void iServer::OnRequestBattleStatus()
{
	if(!m_battle) return;
}

// namespace LSL
