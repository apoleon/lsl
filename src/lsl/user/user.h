#ifndef LSL_USER_H
#define LSL_USER_H

#include "common.h"

#include <lslutils/type_forwards.h>


namespace LSL {

//! actual "Online user"
class User : public CommonUser
{
public:
    User( IServerPtr serv,
          const std::string id = GetNewUserId(),
          const std::string nick = "invalid",
          const std::string country = "",
          const int cpu = DEFAULT_CPU_ID );

	virtual ~User();

	// User interface

	ConstIServerPtr GetServer() const { return m_serv; }

	void Said( const std::string& message ) const;
	void Say( const std::string& message ) const;
	void DoAction( const std::string& message ) const;

	void SendMyUserStatus() const;
	void SetStatus( const UserStatus& status );
	void SetCountry( const std::string& country );

	bool ExecuteSayCommand( const std::string& cmd ) const;

	static std::string GetRankName(UserStatus::RankContainer rank);

    UserStatus::RankContainer GetRank() const;
    std::string GetClan() const;

	//bool operator< ( const ConstUserPtr other ) const { return m_nick < other.GetNick() ; }
	//User& operator= ( const ConstUserPtr other );
protected:
	// User variables
	IServerPtr m_serv;
};

} // namespace LSL {

/**
 * \file battlelist.h
 * \section LICENSE
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#endif // LSL_USER_H
