#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <process.h>

#if !defined (_WIN32)
#include <unistd.h>
#endif

#include "libircclient.h"
#include <windows.h>

#include "utils.h"


#define SERVER "irc.twitch.tv"

#define GOV_VOTE_TIME 60
#define CMD_VOTE_TIME 20
#define SCREEN_W 256
#define SCREEN_H 192

struct irc_ctx_t;

typedef std::vector<std::string> VecStr;
typedef void(*CmdFunc)(const std::string &cmd, irc_ctx_t *ctx, int votes, int presses, int x, int y);

enum GovType
{
    GOV_ANARCHY,
    GOV_DEMOCRACY,
    GOV_DICTATOR
};

struct irc_ctx_t
{
    GovType govtype; // less then zero is democracy, zero or greater is anarchy
    int democracy_num_cmds;

    
    int democracy_votes;
    int anarchy_votes;
    int dictator_votes;
    int gov_vote_timer; //countdown timer for voting a gov
    int cmd_vote_timer; // countdown timer for voting a command (only for democracy)

    CRITICAL_SECTION voteCS;
    std::unordered_set<std::string> cmd_voted;
    std::unordered_set<std::string> gov_voted;
    std::unordered_map<std::string, unsigned int> cmd_votes;
    std::string highest_voted_cmd;
    int highest_voted_cmd_count;

    unsigned long long seconds;
    CRITICAL_SECTION historyCS;
    CircularQueue<std::string, 20> history;
    std::string topic;

    // irc
    std::string  channel;
    std::string nick;
    std::unordered_map<std::string, CommandToken> valid_cmds;

    HFONT font;
    KeyPressQueueShared *keypressQ;
    HANDLE maphandle;
    HANDLE mutexhandle;

    irc_ctx_t();
};

irc_ctx_t gctx;

void split(const std::string &s, char delim, VecStr &elems) {
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        if (!item.empty())
            elems.push_back(item);
    }
}



irc_ctx_t::irc_ctx_t()
: govtype(GOV_ANARCHY), democracy_num_cmds(1), democracy_votes(0), anarchy_votes(0), dictator_votes(0), gov_vote_timer(0), cmd_vote_timer(0), highest_voted_cmd_count(0), seconds(1), font(0), keypressQ(0), maphandle(0), mutexhandle(0)
{
    valid_cmds["up"] = UP;
    valid_cmds["down"] = DOWN;
    valid_cmds["left"] = LEFT;
    valid_cmds["right"] = RIGHT;
    valid_cmds["l"] = L;
    valid_cmds["r"] = R;
    valid_cmds["a"] = A;
    valid_cmds["b"] = B;
    valid_cmds["x"] = X;
    valid_cmds["y"] = Y;
    valid_cmds["start"] = START;
    valid_cmds["select"] = SELECT;
    //valid_cmds["lid"];
    valid_cmds["xNyN"] = XNYN;
    valid_cmds["anarchy"] = ANARCHY;
    valid_cmds["democracy"] = DEMOCRACY;
    //valid_cmds["dictator"];
    valid_cmds["red"] = RED;
    valid_cmds["blue"] = BLUE;
}

std::string parse_cmd(const std::string &rawcmd,int &votes, int &presses, int &x, int &y)
{
    if (rawcmd[0] == 'a' || rawcmd[0] == 'r' || rawcmd[0] == 'b')
    {
        return rawcmd;
    }

    x = y = 0;
    votes = presses = 1;

    bool have1 = false;
    bool onlypresses = false;
    bool need_y = false;

    if (rawcmd[0] == '-')
    {
        votes = -1;
        have1 = true;
    }
    else if (rawcmd[0] == '+')
    {
        votes = 1;
        have1 = true;
    }
    else if (rawcmd[0] >= '1' && rawcmd[0] <= '9')
    {
        presses = rawcmd[0] - '0';
        onlypresses = true;
    }
    else if (rawcmd[0] == 'x')
    {
        if (rawcmd.size() >= 2)
        {
            need_y = true;
        }
    }

    bool have2 = false;
    if (have1 && rawcmd.size() >= 2)
    {
        if (rawcmd[1] >= '1' && rawcmd[1] <= '9')
        {
            presses = rawcmd[1] - '0';
            have2 = true;
        }
        else if (rawcmd[1] == 'x')
        {
            if (rawcmd.size() >= 3)
            {
                need_y = true;
            }
        }
    }

    if (have2 && rawcmd.size() >= 3)
    {
        if (rawcmd[2] == 'x')
        {
            if (rawcmd.size() >= 4)
            {
                need_y = true;
            }
        }
    }

    std::string result;

    if (have2)
    {
        result = rawcmd.substr(2);
    }
    else if (have1 || onlypresses)
    {
        result = rawcmd.substr(1);
    }
    else
    {
        result = rawcmd;
    }

    if (need_y)
    {
        // parse xNyN params
        printf("%s\n", result.c_str());
        int xlen = 0;
        int ylen = 0;
        while (result[xlen] != 'y' && result[xlen] != 0)
        {
            xlen++;
        }
        while (result[xlen+ylen+1] != 0)
        {
            ylen++;
        }
        if ((xlen-1) > 0 && ylen > 0)
        {
            std::string xpart = result.substr(1, xlen - 1);
            std::string ypart = result.substr(xlen + 1, ylen);

            result = "xNyN";

            // perform conversion
            char *endp;
            x = (int)strtol(xpart.c_str(), &endp, 10);
            if (*endp != 0) result = "!";
            y = (int)strtol(ypart.c_str(), &endp, 10);
            if (*endp != 0) result = "!";
        }
        else
        {
            result = "!";
        }
    }

    return result;

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

    In democracy mode, xNyN commands within a distance of 10 pixels are treated as the same command,
    futhermore, the xNyN command that initiated the vote will get choosen as the winner
    Each command may optionally be prefixed with a - for downvote or a + for upvote, if a +/- is not specified the default is an upvote

    Special commands
    - anarchy/red (vote for anarchy gov)
    - democracy/blue (vote for democracy gov)
    - dictator (vote for dictator gov, the moderators will play the game briefly before giving up control,
    this command is useless if there are no moderators online)


    In anarchy mode all commands will get executed immediately.

    In democracy mode, a 10 second voting period takes place

    In dictator mode, only the moderators are allowed to input commands, they may give up control by switching to another government,
    if no commands were issued during the last 30 seconds, go back to anarchy government.
    */
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

    // handle commands here

    int votes; 
    int presses; 
    int x; 
    int y;
    std::string cmd = parse_cmd(params[1], votes, presses, x, y);
    if (!cmd.empty())
    {
        auto fit = ctx->valid_cmds.find(cmd);
        if (fit != ctx->valid_cmds.cend())
        {
            CommandToken cmdtok = fit->second;

            char nick[256]; nick[0] = 0;
            irc_target_get_nick(origin, nick, sizeof(nick));
            std::string nickstr = nick;

            if (x < SCREEN_W && y < SCREEN_H)
            {
                bool justvoted_gov = false, justvoted_cmd = false;

                if (cmdtok == ANARCHY || cmdtok == RED)
                {
                    EnterCriticalSection(&ctx->voteCS);
                    if (ctx->gov_voted.find(nickstr) == ctx->gov_voted.end())
                    {
                        ctx->anarchy_votes++;
                        justvoted_gov = true;
                    }
                    LeaveCriticalSection(&ctx->voteCS);
                }
                else if (cmdtok == DEMOCRACY || cmdtok == BLUE)
                {
                    EnterCriticalSection(&ctx->voteCS);
                    if (ctx->gov_voted.find(nickstr) == ctx->gov_voted.end())
                    {
                        ctx->democracy_votes++;
                        justvoted_gov = true;
                    }
                    LeaveCriticalSection(&ctx->voteCS);
                }
                else if (ctx->govtype == GOV_DEMOCRACY)
                {
                    EnterCriticalSection(&ctx->voteCS);
                    if (ctx->cmd_voted.find(nickstr) != ctx->cmd_voted.end())
                    {
                        std::string cmdStr = params[1];
                        auto cmdIt = ctx->cmd_votes.find(cmdStr);
                        if (cmdIt == ctx->cmd_votes.end())
                        {
                            ctx->cmd_votes[cmdStr] = 1;
                            cmdIt = ctx->cmd_votes.find(cmdStr);
                        }
                        else
                        {
                            cmdIt->second++;
                        }

                        if (ctx->highest_voted_cmd_count < cmdIt->second)
                        {
                            ctx->highest_voted_cmd_count = cmdIt->second;
                            ctx->highest_voted_cmd = cmdIt->first;
                        }
                        justvoted_cmd = true;
                    }
                    LeaveCriticalSection(&ctx->voteCS);
                }

                if (ctx->govtype == GOV_ANARCHY) // TODO check for last cmd won
                {
                    if (shared_mutex_enter(ctx->mutexhandle))
                    {
                        EnterCriticalSection(&ctx->historyCS);
                        if (nickstr.size() > 30)
                        {
                            nickstr = nickstr.substr(0, 30);
                        }
                        ctx->history.push(nickstr + "  " + cmd);
                        LeaveCriticalSection(&ctx->historyCS);

                        KeyPress kp;
                        kp.cmd = fit->second;
                        kp.presses = presses;
                        kp.x = x;
                        kp.y = y;
                        ctx->keypressQ->push(kp);
                        shared_mutex_exit(ctx->mutexhandle);
                    }
                    else
                    {
                        printf("ERROR: failed to grab mutex!");
                    }
                }

                if (justvoted_gov)
                {
                    EnterCriticalSection(&ctx->voteCS);
                    ctx->gov_voted.insert(nickstr);
                    LeaveCriticalSection(&ctx->voteCS);
                }
                if (justvoted_cmd)
                {
                    EnterCriticalSection(&ctx->voteCS);
                    ctx->cmd_voted.insert(nickstr);
                    LeaveCriticalSection(&ctx->voteCS);
                }
            }
        }
    }
    
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

BOOL CALLBACK find_window_cb(HWND hwnd, LPARAM lParam) 
{
    void **result = (void**)lParam;
    static char buffer[256];

    GetWindowTextA(hwnd, buffer, 256);
    if (strstr(buffer, (const char*)result[0])) {
        
        result[1] = (void*)hwnd;
        return FALSE;
    }

    return TRUE;
}

HWND find_window(const std::string &query)
{
    void *result[] = {(char*)query.c_str(), 0};
    EnumWindows(find_window_cb, (LPARAM)result);
    return (HWND)result[1];
}

void parse_time(unsigned long long seconds, int &day, int &hour, int &min, int &sec)
{
    day = seconds / (24*60*60);
    hour = seconds % (24*60*60) / (60*60);
    min = seconds % (60*60) / 60;
    sec = seconds % 60;
}

void writetext(HDC hdc, int x, int y, const char *fmt,...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    TextOutA(hdc, x, y, buf, len);
}
LRESULT CALLBACK scoreboard_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_PAINT)
    {
        int day, hour, min, sec;
        parse_time(gctx.seconds, day, hour, min, sec);

        // set graphics stuff
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);
        SelectObject(hdc, gctx.font);
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(0, 0, 0));
        SetTextColor(hdc, RGB(255, 255, 255));
        int totalvotes = gctx.democracy_votes + gctx.anarchy_votes;
        int anarchy_percent = 0;
        int democracy_percent = 0;
        if (totalvotes > 0)
        {
            anarchy_percent = (int)((double(gctx.anarchy_votes) / totalvotes)*100.0);
            democracy_percent = (int)((double(gctx.democracy_votes) / totalvotes)*100.0);
        }
        writetext(hdc, 0, 0, "[Elapsed: %dd %dh %dm %ds]  [Anarchy: %d%%]  [Democracy: %d%%]", day, hour, min, sec, anarchy_percent, democracy_percent);
        if (gctx.govtype == GOV_DEMOCRACY)
        {
            writetext(hdc, 0, 35, "Democracy. Plan ahead");
        }
        else
        {
            writetext(hdc, 0, 35, "Anarchy! Go crazy!");
        }
        writetext(hdc, 0, 65, "Topic: %s", gctx.topic.c_str());
        if (gctx.govtype == GOV_DEMOCRACY)
        {
            writetext(hdc, 0, 95, "Winner: %s", gctx.highest_voted_cmd.c_str());
        }
        EndPaint(hwnd, &paint);
    }
    else if (msg == WM_TIMER)
    {
        if (wParam == 1337)
        {
            gctx.seconds++;

            // tally up votes and perform action
            if (gctx.gov_vote_timer == 0)
            {
                if (gctx.democracy_votes > gctx.anarchy_votes)
                {
                    gctx.govtype = GOV_DEMOCRACY;
                }
                else
                {
                    gctx.govtype = GOV_ANARCHY;
                }

                gctx.gov_vote_timer = GOV_VOTE_TIME;
                gctx.democracy_votes = 0;
                gctx.anarchy_votes = 0;
                EnterCriticalSection(&gctx.voteCS);
                gctx.gov_voted.clear();
                LeaveCriticalSection(&gctx.voteCS);
            }
            else
                gctx.gov_vote_timer--;

            if (gctx.govtype == GOV_DEMOCRACY)
            {
                if (gctx.cmd_vote_timer == 0)
                {

                    gctx.cmd_vote_timer = CMD_VOTE_TIME;
                    EnterCriticalSection(&gctx.voteCS);
                    gctx.cmd_voted.clear();
                    gctx.cmd_votes.clear();
                    gctx.highest_voted_cmd_count = 0;
                    LeaveCriticalSection(&gctx.voteCS);
                }
                else
                    gctx.cmd_vote_timer--;
            }
        }
        else //1338
            InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (msg == WM_CLOSE)
    {
        exit(0);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

unsigned int __stdcall create_scoreboard_window(void *phwnd)
{
    static char szAppName[] = "Scoreboard";
    HWND         hwnd;
    MSG          msg;
    WNDCLASSA     wndclass;

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = scoreboard_wndproc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = GetModuleHandle(0);
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;

    if (!RegisterClassA(&wndclass))
    {
        MessageBoxA(NULL, "This program requires Windows NT!",
            szAppName, MB_ICONERROR);
        *(HWND*)phwnd = 0;
        return 0;
    }

    *(HWND*)phwnd = hwnd = CreateWindowA(szAppName, "Scoreboard",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        854, 240,
        NULL, NULL, GetModuleHandle(0), NULL);

    // start timer with precision of 1 second
    SetTimer(hwnd, 1337, 1000, 0);

    // screen updates at 10fps
    SetTimer(hwnd, 1338, 1000/10, 0);

    // create font
    gctx.font = CreateFontA(32, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, "Segoe UI");

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
    
}
HWND create_scoreboard()
{
    HWND hwnd = (HWND)-1;
    _beginthreadex(0, 0, create_scoreboard_window, &hwnd, 0, 0);
    while (hwnd == (HWND)-1){ Sleep(1); }
    return hwnd;
}

void on_exit_cleanup()
{
    if (gctx.keypressQ)
    {
        close_shared_mem(gctx.maphandle, gctx.keypressQ);
    }
    if (gctx.mutexhandle)
    {
        shared_mutex_close(gctx.mutexhandle);
    }
}

int main(int argc, char **argv)
{

    if (argc == 2)
    {
        irc_ctx_t ctx;
        int votes;
        int presses;
        int x;
        int y;
        std::string cmd = parse_cmd(argv[1], votes, presses, x, y);
        printf("cmd=%s, votes=%d, presses=%d, x=%d, y=%d\n", cmd.c_str(), votes, presses, x, y);
        printf("valid=%p\n", ctx.valid_cmds.find(cmd) != ctx.valid_cmds.cend() ? 1 : 0);
        HWND hwnd = find_window("DeSmuME");
        if (hwnd)
        {
            /*INPUT input;
            input.type = INPUT_KEYBOARD;
            input.ki.dwFlags = 0;
            input.ki.wScan = 0;
            input.ki.dwExtraInfo = GetMessageExtraInfo();
            input.ki.time = 0;
            input.ki.wVk = 'W';
            SetForegroundWindow(hwnd);
            //Sleep(100);
            SendInput(1, &input, sizeof(input));
            Sleep(30);
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(input));*/

            PostMessageA(hwnd, WM_KEYDOWN, 'W', 1);
            Sleep(30);
            PostMessageA(hwnd, WM_CHAR, 'w', 1);
            Sleep(60);
            PostMessageA(hwnd, WM_KEYUP, 'W', 0xC0000001);
        }

        return 0;
    }
    
    // create shared mem and mutex
    atexit(on_exit_cleanup);
    gctx.mutexhandle = shared_mutex_create(APP_KEY_MUTEX);
    if (!gctx.mutexhandle)
    {
        printf("Failed to create mutex");
        return -1;
    }
    gctx.keypressQ = (KeyPressQueueShared*)create_shared_mem(APP_KEY_MEM, sizeof(KeyPressQueueShared), &gctx.maphandle);
    if (!gctx.keypressQ)
    {
        printf("Failed to create shared mem");
        return -1;
    }

    InitializeCriticalSection(&gctx.voteCS);
    InitializeCriticalSection(&gctx.historyCS);

    HWND hwndScore = create_scoreboard();

    irc_callbacks_t	callbacks;
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


    irc_set_ctx(s, &gctx);

    // get nick and password
    // line 1 = channel
    // line 2 = nick
    // line 3 = password
    std::string password;
    std::ifstream f("everyoneplays.txt");
    if (f) 
    {
        std::getline(f, gctx.channel);
        std::getline(f, gctx.nick);
        std::getline(f, password);
    }
    else
    {
        printf("Failed to open everyoneplays.txt");
        return 1;
    }

    // Initiate the IRC server connection
    if (irc_connect(s, SERVER, port, password.c_str(), gctx.nick.c_str(), 0, 0))
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
