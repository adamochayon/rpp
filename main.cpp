//
// Created by Dan The Man on 9/16/2018.
//

#ifndef _WIN32
#warning Bi-directional io is only supported on Windows.

#else

#include <Windows.h>
#include <string>
#include <stack>

#define BACKSPACE 8
#define TAB 9
#define ENTER 13
#define SPACE 32

using namespace std;

CONSOLE_SCREEN_BUFFER_INFO cInfo;
HANDLE hStdout, hStdin;
DWORD oldMode;

struct run
{
    wstring s = L"";
    bool rtl = false;
    unsigned int whitespace = 0;
};

void exit(int c)
{
    SetConsoleMode(hStdin, oldMode);
    _exit(c);
}

void error()
{
    exit(99000 + (int)GetLastError());
}

void writeStr(wstring s)
{
    DWORD out;
    if (!WriteConsoleW(hStdout, s.c_str(), (DWORD)s.size(), &out, NULL))
        error();
    if (out != s.size())
        exit(10000 + (int)out);
}

wchar_t getWChar()
{
    wchar_t ch;
    DWORD read;
    if (!ReadConsoleW(hStdin, &ch, 1, &read, NULL))
        error();
    if (read != 1)
        exit(11000 + (int)read);
    return ch;
}

void writeWChar(wchar_t ch)
{
    DWORD written;
    if (!WriteConsoleW(hStdout, &ch, 1, &written, NULL))
        error();
    if (written != 1)
        exit(12000 + (int)written);
}

wstring utf8ToUtf16(string s)
{
    auto chars = (size_t)MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.size(), NULL, 0);
    wchar_t *ws = (wchar_t *)malloc(chars * sizeof(wchar_t));
    int converted = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.size(), ws, chars);
    if (converted != chars)
        exit(13000 + converted);
    return wstring(ws, chars);
}

string utf16ToUtf8(wstring ws)
{
    auto buffSize = (size_t)WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), NULL, 0, NULL, NULL);
    char *s = (char *)malloc(buffSize);
    int converted = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), s, buffSize, NULL, NULL);
    if (converted != buffSize)
        exit(14000 + converted);
    return string(s, buffSize);
}

inline short windowWidth()
{
    return cInfo.dwSize.X;
}

inline short cursorLocation()
{
    GetConsoleScreenBufferInfo(hStdout, &cInfo);
    return cInfo.dwCursorPosition.X;
}

inline short cursorRow()
{
    GetConsoleScreenBufferInfo(hStdout, &cInfo);
    return cInfo.dwCursorPosition.Y;
}

void moveCursor(int x)
{
    if (!x)
        return;

    GetConsoleScreenBufferInfo(hStdout, &cInfo);
    cInfo.dwCursorPosition.X += x;
    if (cInfo.dwCursorPosition.X < 0)
    {
        cInfo.dwCursorPosition.X += cInfo.dwSize.X;
        cInfo.dwCursorPosition.Y--;
    }
    else if (cInfo.dwCursorPosition.X >= cInfo.dwSize.X)
    {
        cInfo.dwCursorPosition.X -= cInfo.dwSize.X;
        cInfo.dwCursorPosition.Y++;
    }
    SetConsoleCursorPosition(hStdout, cInfo.dwCursorPosition);
}

void moveCursorRTL(int x)
{
    // wrap cursor in a right-to-left fashion
    if (!x)
        return;

    GetConsoleScreenBufferInfo(hStdout, &cInfo);
    cInfo.dwCursorPosition.X += x;
    // printf(" ========= %d %d ======== ", cInfo.dwCursorPosition.X, cInfo.dwSize.X);
    if (cInfo.dwCursorPosition.X < 0)
    {
        cInfo.dwCursorPosition.X += cInfo.dwSize.X;
        cInfo.dwCursorPosition.Y++;
    }
    else if (cInfo.dwCursorPosition.X >= cInfo.dwSize.X)
    {
        cInfo.dwCursorPosition.X -= cInfo.dwSize.X;
        cInfo.dwCursorPosition.Y--;
    }
    SetConsoleCursorPosition(hStdout, cInfo.dwCursorPosition);
}

void setCursor(short x, short y)
{
    GetConsoleScreenBufferInfo(hStdout, &cInfo);
    cInfo.dwCursorPosition.X = x;
    cInfo.dwCursorPosition.Y = y;
    SetConsoleCursorPosition(hStdout, cInfo.dwCursorPosition);
}

bool isHebrew(wchar_t ch)
{
    return 1488 <= ch && ch <= 1514;
}

inline void reverse(wstring &from, wstring &to)
{
    to = L"";
    for (auto pch = from.end() - 1; pch >= from.begin(); pch--)
        to += *pch;
}

wstring complexInputLTR()
{
    /*
     * States:
     *  empty - current.s = "", runs size is 0, tmpStr is "", current.rtl does'nt matter.
     *  rtl run - cursor is at end of run ($לכל מי שכאן).
     *  rtl run and whitespace - cursor is at end of txt after whitespace (לכל מי שכאן  $).
     *      also tmpStr contains whitespace characters.
     */
    auto runs = stack<run>();
    bool whitespace = true;
    wstring tmpStr = L"";

    auto current = run();
    current.rtl = false;

    while (wchar_t ch = getWChar())
    {
        switch (ch)
        {
        case BACKSPACE:
            if (tmpStr.empty() && current.s.empty() && runs.empty())
                break;

            if (whitespace)
            {
                if (!tmpStr.empty())
                { // prevent backspace after hebrew run deletion
                    tmpStr = tmpStr.substr(0, tmpStr.length() - 1);
                    writeWChar(BACKSPACE);
                    if (tmpStr.empty())
                    {
                        whitespace = false;
                        current.whitespace = 0;
                        if (current.rtl && !current.s.empty())
                            moveCursor(-current.s.length());
                    }
                }
            }
            else
            {
                current.s = current.s.substr(0, current.s.length() - 1);
                if (current.rtl)
                {
                    reverse(current.s, tmpStr);
                    writeStr(tmpStr + L" ");
                    moveCursor(-current.s.length() - 1);
                }
                else
                {
                    moveCursor(-1);
                    writeWChar(SPACE);
                    moveCursor(-1);
                }
            }

            if (!whitespace && current.s.empty())
            { // reached end of current run
                if (!runs.empty())
                {
                    current = runs.top();
                    runs.pop();

                    // prev run ends with whitespace
                    whitespace = true;
                    if (current.whitespace)
                    {
                        tmpStr = current.s.substr(current.s.length() - current.whitespace, current.whitespace);
                        current.s = current.s.substr(0, current.s.length() - current.whitespace);
                    }
                    else
                    {
                        tmpStr = L"";
                    }
                }
                else
                {
                    // empty state
                    whitespace = true;
                    tmpStr = L"";
                }
            }
            break;
        case ENTER:
            if (!whitespace && current.rtl)
                moveCursor(current.s.length() + 1);
            
            tmpStr = L"";
            runs.push(current);
            while (runs.size())
            {
                current = runs.top();
                tmpStr = current.s + tmpStr;
                runs.pop();
            }
            writeWChar(ENTER);
            return tmpStr;
        case TAB:
        case SPACE:
            if (!whitespace)
            {
                tmpStr = L"";
                if (current.rtl)
                {
                    moveCursor(current.s.length());
                }
            }
            writeWChar(SPACE);
            tmpStr += SPACE;
            whitespace = true;
            break;
        default:
            if (whitespace)
            {
                current.s += tmpStr;
                current.whitespace = tmpStr.length();
                if (current.rtl)
                {
                    if (isHebrew(ch))
                    {
                        moveCursor(-current.s.length());
                    }
                    else
                    {
                        runs.push(current);
                        current = run();
                    }
                }
                else
                {
                    if (isHebrew(ch))
                    {
                        runs.push(current);
                        current = run();
                        current.rtl = true;
                    }
                }
                whitespace = false;
            }

            current.s += ch;
            if (current.rtl)
            {
                reverse(current.s, tmpStr);
                writeStr(tmpStr);
                moveCursor(-current.s.length());
            }
            else
            {
                writeWChar(ch);
            }
        }
    }

    return L"";
}

wstring complexInputRTL()
{
    /*
     * States:
     *  empty - current.s = "", runs size is 0, tmpStr is "", current.rtl does'nt matter.
     *  rtl run - cursor is at left end of run ($לכל מי שכאן).
     *  rtl run and whitespace - cursor is at the left of the txt ($    לכל מי שכאן).
     *      also tmpStr contains whitespace characters.
     *  otherwise cursor is at right end of ltr run.
     *  ltr run - cursor is at right side of run (hello$ יאללה מגניב).
     *  ltr run and whitespace - whitespace at left end of run ($   hello שלום וגם).
     */
    auto runs = stack<run>();
    bool whitespace = true;
    wstring tmpStr = L"";
    auto clearCell = wstring{SPACE, BACKSPACE};

    auto current = run();
    current.rtl = false;

    while (wchar_t ch = getWChar())
    {
        switch (ch)
        {
        case BACKSPACE:
            if (tmpStr.empty() && current.s.empty() && runs.empty())
                break;

            if (whitespace)
            {
                if (!tmpStr.empty())
                { // prevent backspace after run deletion
                    tmpStr = tmpStr.substr(0, tmpStr.length() - 1);
                    writeStr(clearCell);
                    moveCursorRTL(1);
                    if (tmpStr.empty())
                    {
                        whitespace = false;
                        current.whitespace = 0;
                        if (!current.rtl && !current.s.empty())
                            moveCursorRTL(current.s.length());
                    }
                }
            }
            else
            {
                current.s = current.s.substr(0, current.s.length() - 1);
                if (current.rtl) {
                    moveCursorRTL(1);
                    writeStr(clearCell);
                }
                else
                {
                    if (current.s.length() <= cursorLocation()) {
                        moveCursorRTL(-current.s.length());
                        writeStr(L" " + current.s + L"\b");
                    } else {
                        moveCursorRTL(-current.s.length());
                        volatile auto tailOffset = current.s.length() - (windowWidth() - cursorLocation()) + 1;
                        writeStr(L" " + current.s.substr(tailOffset) + L"\b");
                        moveCursorRTL(tailOffset);
                    }
                }
            }

            if (!whitespace && current.s.empty())
            { // reached end of current run
                if (!runs.empty())
                {
                    current = runs.top();
                    runs.pop();

                    // prev run ends with whitespace
                    whitespace = true;
                    if (current.whitespace)
                    {
                        tmpStr = current.s.substr(current.s.length() - current.whitespace, current.whitespace);
                        current.s = current.s.substr(0, current.s.length() - current.whitespace);
                    }
                    else
                        tmpStr = L"";
                }
                else
                {
                    // empty state
                    whitespace = true;
                    tmpStr = L"";
                }
            }
            break;
        case ENTER:
            if (!whitespace && !current.rtl)
                moveCursorRTL(-current.s.length());
            setCursor(0, cursorRow() + 1);

            tmpStr = L"";
            runs.push(current);
            while (runs.size())
            {
                current = runs.top();
                tmpStr = current.s + tmpStr;
                runs.pop();
            }
            return tmpStr;
        case TAB:
        case SPACE:
        {
            int offset = -1;
            if (!whitespace)
            {
                tmpStr = L"";
                if (!current.rtl)
                    offset -= current.s.length();
            }
            moveCursorRTL(offset);
            tmpStr += SPACE;
            whitespace = true;
            break;
        }
        default:
            if (whitespace)
            {
                current.s += tmpStr;
                current.whitespace = tmpStr.length();
                if (current.rtl)
                {
                    if (!isHebrew(ch))
                    {
                        runs.push(current);
                        current = run();
                    }
                }
                else
                {
                    if (isHebrew(ch))
                    {
                        runs.push(current);
                        current = run();
                        current.rtl = true;
                    }
                    else
                        moveCursorRTL(current.s.length());
                }
                whitespace = false;
            }

            current.s += ch;
            if (current.rtl)
            {
                writeStr(clearCell);
                moveCursorRTL(-1);
            }
            else
            {
                auto loc = cursorLocation();
                auto row = cursorRow();
                if (loc + 1 < current.s.length()) {
                    moveCursor(-loc);
                    writeStr(current.s.substr(0, loc));
                    auto i = loc + 1;
                    auto j = row;
                    while (i < current.s.length()) { // print lines
                        j++;
                        volatile auto tail = current.s.length() - i;
                        if (tail <= windowWidth())
                            setCursor(windowWidth() - tail, j);
                        else
                            setCursor(0, j);
                        writeStr(current.s.substr(i, windowWidth()));
                        i += windowWidth();
                    }
                    setCursor(loc, row);
                } else {
                    moveCursor(1 - current.s.length());
                    writeStr(current.s);
                    moveCursor(-1);
                }
            }
        }
    }

    return L"";
}

void complexOutputLTR(wstring s, bool endLine)
{
    auto runs = stack<wstring *>();
    auto current = new wstring();
    bool whitespace = true;
    bool rtl = false;
    wstring tmpStr;

    for (auto pch = s.begin(); pch < s.end(); pch++)
    {
        auto ch = *pch;
        switch (ch)
        {
        case ENTER:
        case TAB:
        case SPACE:
            whitespace = true;
            tmpStr += ch;
            break;
        default:
            if (whitespace)
            {
                if (rtl)
                {
                    if (!isHebrew(ch))
                    {
                        wstring rev;
                        reverse(*current, rev);
                        runs.push(new wstring(rev));
                        *current = tmpStr;
                        rtl = false;
                    }
                    else
                        *current += tmpStr;
                }
                else
                {
                    *current += tmpStr;
                    if (isHebrew(ch))
                    {
                        runs.push(current);
                        current = new wstring();
                        rtl = true;
                    }
                }
                tmpStr = L"";
                whitespace = false;
            }

            *current += ch;
        }
    }

    if (whitespace)
        *current += tmpStr;
    if (rtl)
    {
        reverse(*current, tmpStr);
        delete current;
        runs.push(new wstring(tmpStr));
    }
    else
        runs.push(current);

    tmpStr = L"";
    while (runs.size())
    {
        current = runs.top();
        tmpStr = *current + tmpStr;
        delete current;
        runs.pop();
    }
    if (endLine)
        tmpStr += wstring{ENTER};
    writeStr(tmpStr);
}

void complexOutputRTL(wstring s, bool endLine)
{
    auto runs = stack<wstring *>();
    auto current = new wstring();
    bool whitespace = true;
    wstring tmpStr = L"";
    bool rtl = false;

    for (auto pch = s.begin(); pch < s.end(); pch++)
    {
        auto ch = *pch;
        switch (ch)
        {
        case ENTER:
        case TAB:
        case SPACE:
            whitespace = true;
            tmpStr += ch;
            break;
        default:
            if (whitespace)
            {
                if (rtl)
                {
                    *current += tmpStr;
                    if (!isHebrew(ch))
                    {
                        reverse(*current, tmpStr);
                        runs.push(new wstring(tmpStr));
                        *current = L"";
                        rtl = false;
                    }
                }
                else
                {
                    if (isHebrew(ch))
                    {
                        runs.push(current);
                        current = new wstring(tmpStr);
                        rtl = true;
                    }
                    else
                        *current += tmpStr;
                }
                tmpStr = L"";
                whitespace = false;
            }

            *current += ch;
        }
    }

    if (whitespace)
        *current += tmpStr;
    if (rtl)
    {
        reverse(*current, tmpStr);
        delete current;
        runs.push(new wstring(tmpStr));
    }
    else
        runs.push(current);

    tmpStr = L"";
    while (runs.size())
    {
        current = runs.top();
        tmpStr += *current;
        delete current;
        runs.pop();
    }

    if (tmpStr.length() > cursorLocation())
    {
        unsigned int nextLines = tmpStr.length() - cursorLocation();
        // position for str ending
        setCursor(1 + windowWidth() - (nextLines % windowWidth()), cursorRow() + 1 + nextLines / windowWidth());

        // write last line
        writeStr(tmpStr.substr(0, tmpStr.length() % windowWidth()));
        tmpStr = tmpStr.substr(tmpStr.length() % windowWidth());
        setCursor(0, cursorRow() - (short)2);

        while (tmpStr.length() > windowWidth())
        {
            writeStr(tmpStr.substr(0, (unsigned int)windowWidth()));
            tmpStr = tmpStr.substr((unsigned int)windowWidth());
            setCursor(0, cursorRow() - (short)2);
        }

        // write first line
        writeStr(tmpStr);
        setCursor(windowWidth() - (nextLines % windowWidth()), cursorRow() + (short)nextLines / windowWidth());
    }
    else
    {
        moveCursor(-tmpStr.length() + 1);
        writeStr(tmpStr);
        moveCursor(-tmpStr.length());
    }
    if (endLine)
        setCursor(0, cursorRow() + 1);
}

#endif

void simpleInput()
{
    wstring txt = L"";
    while (wchar_t ch = getWChar())
    {
        switch (ch)
        {
        case BACKSPACE:
            if (!txt.length())
                continue;
            txt = txt.substr(0, txt.length() - 1);
            writeStr(L"\b \b");
            moveCursor(-txt.length());
            break;
        default:
            moveCursor(-txt.length());
            txt += ch;
        }
        for (auto pch = txt.end() - 1; pch >= txt.begin(); pch--)
            writeWChar(*pch);
    }
}

int main()
{
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &oldMode);
    SetConsoleMode(hStdin, ENABLE_PROCESSED_INPUT);

    // setCursor(cInfo.dwSize.X - (short)1, cursorRow());
    // complexInputRTL();
    complexInputLTR();

    SetConsoleMode(hStdin, oldMode);
}