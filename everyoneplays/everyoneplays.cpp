#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#if !defined (_WIN32)
#include <unistd.h>
#endif

#include "libircclient.h"
#include <windows.h>

#define SERVER "irc.twitch.tv"

struct irc_ctx_t;

typedef std::vector<std::string> VecStr;
typedef void(*CmdFunc)(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y);

enum GovType
{
    ANARCHY,
    DEMOCRACY,
    DICTATOR
};
struct irc_ctx_t
{
    GovType govtype; // less then zero is democracy, zero or greater is anarchy
    int democracy_num_cmds;

    int democracy_votes;
    int anarchy_votes;
    int dictator_votes;

    std::string  channel;
    std::string nick;
    std::unordered_map <std::string, CmdFunc> cmds;

    irc_ctx_t();
};

void split(const std::string &s, char delim, VecStr &elems) {
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        if (!item.empty())
            elems.push_back(item);
    }
}

// controller comands
void up(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void down(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void left(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void right(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void l_shoulder(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void r_shoulder(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void a(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void b(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}

void x(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void y(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void start(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void select(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void lid(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}

void xNyN(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}

// special commands
void anarchy(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void democracy(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}
void dictator(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y)
{

}


irc_ctx_t::irc_ctx_t()
    : govtype(ANARCHY), democracy_num_cmds(1), democracy_votes(0), anarchy_votes(0), dictator_votes(0)
{
    cmds["up"] = up;
    cmds["down"] = down;
    cmds["left"] = left;
    cmds["right"] = right;
    cmds["l"] = l_shoulder;
    cmds["r"] = r_shoulder;
    cmds["a"] = a;
    cmds["b"] = b;
    cmds["x"] = x;
    cmds["y"] = y;
    cmds["start"] = start;
    cmds["select"] = select;
    cmds["lid"] = lid;
    cmds["xNyN"] = xNyN;
    cmds["anarchy"] = anarchy;
    cmds["democracy"] = democracy;
    cmds["dictator"] = dictator;
    cmds["red"] = anarchy;
    cmds["blue"] = democracy;
}

void event_connect(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t * ctx = (irc_ctx_t *)irc_get_ctx(session);
    irc_cmd_join(session, ctx->channel.c_str(), 0);
}


void event_channel(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
    irc_ctx_t * ctx = (irc_ctx_t *)irc_get_ctx(session);
    if (!origin || count != 2)
        return;

    VecStr cmds;
    split(params[1], ' ', cmds);
    
    // handle commands here


    /*
        Controller commands
        - up
        - down
        - left
        - right
        - l
        - r
        - a
        - b
        - x
        - y
        - start
        - select
        - lid
        - xNyN (tap stylus at x,y coordinate, for eg x123y20, x10y30, etc)

        Each controller command may be prefixed with a number between 1 and 9 to indicate the number of presses

        Some examples:
        - up (vote for or press up once)
        - 3up (vote for or press up 3 times)
        - -up (downvote up once)
        - -3up (downvote up 3 times)
        - +up ( same issuing up)
        - +3up (same as issuing 3up)
        - x100y50 (vote to tap the stylus at 100,50 coordinates)
        - 9x100y50 (vote to tap the stylus 9 times at 100,50 coordinates)
        - -x100y50 (downvote to tap the stylus at 100,50 coordinates)
        - -9x100y50 (downvote to tap the stylus 9 times at 100,50 coordinates)

        You may also vote for multiple commands per line, for example:
        - up down -3a 2x
        - a 2x -9x100y50

        In democracy mode, xNyN commands within a distance of 10 pixels are treated as the same command, 
        futhermore, the xNyN command that initiated the vote will get choosen as the winner
        Each command may optionally be prefixed with a - for downvote or a + for upvote, if a +/- is not specified the default is an upvote

        Special commands
        - anarchy/red (vote for anarchy gov)
        - democracy/blue (vote for democracy gov)
        - democracyN/blueN (vote for democracy gov which allows voting for the top most popular N commands, N must be between 1 and 9)
        - dictator (vote for dictator gov, the moderators will play the game briefly before giving up control, 
                    this command is useless if there are no moderators online)


        In anarchy mode all commands will get executed immediately.

        In democracy mode, a 20 second voting period takes place, the top most popular N commands get executed

        In dictator mode, only the moderators are allowed to input commands, they may give up control by switching to another government,
        if no commands were issued during the last 40 seconds, then go back to anarchy government.
    */

}


void event_numeric(irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
    if (event > 400)
    {
        std::string fulltext;
        for (unsigned int i = 0; i < count; i++)
        {
            if (i > 0)
                fulltext += " ";

            fulltext += params[i];
        }

        printf("ERROR %d: %s: %s\n", event, origin ? origin : "?", fulltext.c_str());
    }
}


int main(int argc, char **argv)
{
    irc_callbacks_t	callbacks;
    irc_ctx_t ctx;
    unsigned short port = 6667;

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("Could not start winsock\n");
        return 1;
    }

    // Initialize the callbacks
    memset(&callbacks, 0, sizeof(callbacks));

    // Set up the callbacks we will use
    callbacks.event_connect = event_connect;
    callbacks.event_channel = event_channel;
    callbacks.event_numeric = event_numeric;

    // And create the IRC session; 0 means error
    irc_session_t * s = irc_create_session(&callbacks);

    if (!s)
    {
        printf("Could not create IRC session\n");
        return 1;
    }


    irc_set_ctx(s, &ctx);

    // get nick and password
    // line 1 = channel
    // line 2 = nick
    // line 3 = password
    std::string password;
    std::ifstream f("everyoneplays.txt");
    if (f) 
    {
        std::getline(f, ctx.channel);
        std::getline(f, ctx.nick);
        std::getline(f, password);
    }
    else
    {
        printf("Failed to open everyoneplays.txt");
        return 1;
    }

    // Initiate the IRC server connection
    if (irc_connect(s, SERVER, port, password.c_str(), ctx.nick.c_str(), 0, 0))
    {
        printf("Could not connect: %s\n", irc_strerror(irc_errno(s)));
        return 1;
    }

    // and run into forever loop, generating events
    if (irc_run(s))
    {
        printf("Could not connect or I/O error: %s\n", irc_strerror(irc_errno(s)));
        return 1;
    }

    return 0;
}
