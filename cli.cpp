/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __WINDOWS__
#include <unistd.h>
#endif

#include "node/Node.hpp"
#include "node/Constants.hpp"
#include "node/Utils.hpp"
#include "node/Thread.hpp"
#include "node/Condition.hpp"

using namespace ZeroTier;

static void printHelp(FILE *out,const char *exename)
{
	fprintf(out,"Usage: %s [-switches] <command>"ZT_EOL_S,exename);
	fprintf(out,ZT_EOL_S);
	fprintf(out,"Available switches:"ZT_EOL_S);
	fprintf(out," -c<port>         - Communicate with daemon over this local port"ZT_EOL_S);
	fprintf(out," -t<token>        - Specify token on command line"ZT_EOL_S);
	fprintf(out," -T<file>         - Read token from file"ZT_EOL_S);
	fprintf(out,ZT_EOL_S);
	fprintf(out,"Use the 'help' command to get help from ZeroTier One itself."ZT_EOL_S);
}

static volatile unsigned int numResults = 0;
static Condition doneCondition;

static void resultHandler(void *arg,unsigned long id,const char *line)
{
	++numResults;
	if (strlen(line))
		fprintf(stdout,"%s"ZT_EOL_S,line);
	else doneCondition.signal();
}

int main(int argc,char **argv)
{
	if (argc <= 1) {
		printHelp(stdout,argv[0]);
		return -1;
	}

	std::string authToken;
	std::string command;
	bool pastSwitches = false;
	unsigned int controlPort = 0;
	for(int i=1;i<argc;++i) {
		if ((argv[i][0] == '-')&&(!pastSwitches)) {
			if (strlen(argv[i]) <= 1) {
				printHelp(stdout,argv[0]);
				return -1;
			}
			switch(argv[i][1]) {
				case 'c':
					controlPort = Utils::strToUInt(argv[i] + 2);
					break;
				case 't':
					authToken.assign(argv[i] + 2);
					break;
				case 'T':
					if (!Utils::readFile(argv[i] + 2,authToken)) {
						fprintf(stdout,"FATAL ERROR: unable to read token from '%s'"ZT_EOL_S,argv[i] + 2);
						return -2;
					}
					break;
				default:
					return -1;
			}
		} else {
			pastSwitches = true;
			if (command.length())
				command.push_back(' ');
			command.append(argv[i]);
		}
	}

	if (!authToken.length()) {
		const char *home = getenv("HOME");
		if (home) {
			std::string dotZeroTierAuthToken(home);
			dotZeroTierAuthToken.push_back(ZT_PATH_SEPARATOR);
			dotZeroTierAuthToken.append(".zerotierOneAuthToken");
			if (!Utils::readFile(dotZeroTierAuthToken.c_str(),authToken)) {
#ifndef __WINDOWS__
#ifdef __APPLE__
				const char *systemAuthTokenPath = "/Library/Application Support/ZeroTier/One/authtoken.secret";
#else
				const char *systemAuthTokenPath = "/var/lib/zerotier-one/authtoken.secret";
#endif
				if (!Utils::readFile(systemAuthTokenPath,authToken)) {
					fprintf(stdout,"FATAL ERROR: no token specified on command line and could not read '%s' or '%s'"ZT_EOL_S,dotZeroTierAuthToken.c_str(),systemAuthTokenPath);
					return -2;
				}
#else // __WINDOWS__
				fprintf(stdout,"FATAL ERROR: no token specified on command line and could not read '%s'"ZT_EOL_S,dotZeroTierAuthToken.c_str());
				return -2;
#endif // __WINDOWS__
			}
		}
	}
	if (!authToken.length()) {
		fprintf(stdout,"FATAL ERROR: could not find auth token"ZT_EOL_S);
		return -2;
	}

	Node::LocalClient client(authToken.c_str(),controlPort,&resultHandler,(void *)0);
	client.send(command.c_str());

	doneCondition.wait(1000);

	if (!numResults) {
		fprintf(stdout,"ERROR: no results received. Is ZeroTier One running?"ZT_EOL_S);
		return -1;
	}

	return 0;
}
