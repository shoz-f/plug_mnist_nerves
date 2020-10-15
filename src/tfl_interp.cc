/***  File Header  ************************************************************/
/**
* tfl_interp.cc
*
* Elixir/Erlang Port ext. of tensor flow lite
* @author	   Shozo Fukuda
* @date	create Sat Sep 26 06:26:30 JST 2020
* System	   MINGW64/Windows 10<br>
*
**/
/**************************************************************************{{{*/

#include <unistd.h>
#include <iostream>

#include <cerrno>
#ifdef EINTR
#  define IS_EINTR(x) ((x) == EINTR)
#else
#  define IS_EINTR(x) 0
#endif

#include <iterator>
#include <regex>
#include <string>
using namespace std;

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
using namespace tflite;

struct {
    string mPath;
    string mExe;
    string mTflModel;
} gInfo;

/***  Module Header  ******************************************************}}}*/
/**
* read specific sized byte from stdin
* @par DESCRIPTION
*   <<解説記入>>
*
* @return count of received byte or error code
**/
/**************************************************************************{{{*/
ssize_t
full_read(int fd, void *buf, size_t count)
{
    char *ptr = reinterpret_cast<char *>(buf);

    size_t total = 0;
    while (count > 0) {
        ssize_t n;
        do {
            n = read(fd, buf, count);
        } while (n < 0 && IS_EINTR(errno));

        if (n < 0) {
            //error occured
            return n;
        }
        else if (n == 0) {
            // termination: total is less than required count
            return total;
        }

        total += n;
        ptr += n;
        count -= n;
    }

    return total;
}

/***  Module Header  ******************************************************}}}*/
/**
* write specific sized byte to stdout
* @par DESCRIPTION
*   <<解説記入>>
*
* @return count of sent byte or error code
**/
/**************************************************************************{{{*/
size_t
full_write(int fd, const void *buf, size_t count)
{
    size_t total = 0;
    const char *ptr = (const char *) buf;

    while (count > 0) {
        size_t n = write(fd, ptr, count);

        if (n < 0) {
            // error occured
            return n;
        }
        else if (n == 0) {
            // termination: total is less than required count
            return total;
        }

        total += n;
        ptr += n;
        count -= n;
    }

    return total;
}

/***  Module Header  ******************************************************}}}*/
/**
* receive command packet from Elixir/Erlang
* @par DESCRIPTION
*   receive command packet and store it to "buff"
*
* @retval res >  0  success
* @retval res == 0  termination
* @retval res <  0  error
**/
/**************************************************************************{{{*/
ssize_t
rcv_packet(string& cmd_line)
{
    // receive packet size
    char big_endian[2];
    ssize_t n = full_read(STDIN_FILENO, big_endian, sizeof(big_endian));
    if (n <= 0) {
        return n;
    }
    else if (n < 2) {
        errno = ENODATA;
        return ((ssize_t)-1);
    }
    unsigned short len = (unsigned short)(big_endian[0] << 8 | big_endian[1]);

    // receive packet payload
    unique_ptr<char[]> buff(new char[len]);
    n = full_read(STDIN_FILENO, buff.get(), len);
    if (n <= 0) {
        return n;
    }
    else if (n < len) {
        errno = ENODATA;
        return ((ssize_t)-1);
    }

    // return received command line
    cmd_line.assign(buff.get(), n);
    return n;
}

/***  Module Header  ******************************************************}}}*/
/**
* send result packet to Elixir/Erlang
* @par DESCRIPTION
*   construct message packet and send it to stdout
*
* @return count of sent byte or error code
**/
/**************************************************************************{{{*/
ssize_t
snd_packet(string result)
{
    unsigned short len = result.size();

    char big_endian[2];
    big_endian[0] = 0xff & (len >> 8);
    big_endian[1] = 0xff & (len);
    result.insert(0, big_endian, sizeof(big_endian));

    return full_write(STDOUT_FILENO, result.c_str(), len+2);
}

/***  Module Header  ******************************************************}}}*/
/**
* parse command line string
* @par DESCRIPTION
*   extract command & arguments string from string
*
* @retval command string & vector of arguments
**/
/**************************************************************************{{{*/
string
parse_cmd_line(const string& cmd_line, vector<string>& args)
{
    regex reg(R"(\s+)");
    sregex_token_iterator iter(cmd_line.begin(), cmd_line.end(), reg, -1);
    sregex_token_iterator end;

    string command = *iter++;
    args.assign(iter, end);

    return command;
}

/***  Module Header  ******************************************************}}}*/
/**
* tensor flow lite interpreter
* @par DESCRIPTION
*   <<解説記入>>
**/
/**************************************************************************{{{*/
void
interp(const char* tfl_name)
{
    // initialize tensor flow lite
    unique_ptr<tflite::FlatBufferModel> model =
        tflite::FlatBufferModel::BuildFromFile(tfl_name);

    tflite::ops::builtin::BuiltinOpResolver resolver;
    InterpreterBuilder builder(*model, resolver);
    unique_ptr<Interpreter> interpreter;
    builder(&interpreter);

    interpreter->AllocateTensors();

    // REPL
    for (;;) {
        // receive command packet
        string cmd_line;
        ssize_t n = rcv_packet(cmd_line);
        if (n <= 0) {
            break;
        }

        // parse command line
        vector<string> args;
        const string command = parse_cmd_line(cmd_line, args);

        json result;
        result.clear();
        if (command == "predict") {
            extern void predict(unique_ptr<Interpreter>& interpreter, const vector<string>& args, json& result);
            predict(interpreter, args, result);
        }
        else if (command == "info") {
            result["exe"]   = gInfo.mExe;
            result["model"] = gInfo.mTflModel;
        }
        else {
            result["unknown"] = command;
        }

        n = snd_packet(result.dump());
        if (n <= 0) {
            break;
        }
    }
}

/***  Module Header  ******************************************************}}}*/
/**
* tensor flow lite for Elixir/Erlang Port ext.
* @par DESCRIPTION
*   Elixir/Erlang Port extension (experimental)
*
* @return exit status
**/
/**************************************************************************{{{*/
int
main(int argc, char* argv[])
{
    if (argc < 2) {
        // argument error
        cerr << "expect <model file>\n";
        return 1;
    }
    
    // save exe infomations
    gInfo.mExe.assign(argv[0]);
    gInfo.mTflModel.assign(argv[1]);

    interp(argv[1]);
    
    return 0;
}

/*** tfl_interp.cc ********************************************************}}}*/