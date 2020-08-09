#include<bits/stdc++.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

using namespace std;

void preprocess(string line, vector<string>& commands)
{
    int parser = 0, start = 0, end = 0;
    
    // remove the initial spaces
    while(parser != line.size() && line[parser] == ' ') parser++;

    // parse the line and extract out commands
    start = parser;
    end = parser;
    while(parser != line.size())
    {
        if(line[parser] == ' ')
        {
            if(line[start] != ' ') commands.push_back(line.substr(start, end - start + 1));
            start = parser + 1;
            end = parser + 1;
        }
        else end = parser;
        parser++;
    }
    if(start < line.size() && end < line.size()) commands.push_back(line.substr(start, end - start + 1));

    return;
}

void run_command(vector<string>& commandlets, int in_fd, int out_fd)
{
    pid_t child_pid;
    bool wait_flag = true, in_rd = false, out_rd = false, bg = false;
    int end = 0;
    string in_file, out_file;
    
    // detect the redirections
    for(int i = 0; i < commandlets.size(); i++)
    {
        if(commandlets[i] == "<")
        {
            in_rd = true;
            in_file = commandlets[i + 1];
        }
        else if(commandlets[i] == ">")
        {
            out_rd = true;
            out_file = commandlets[i + 1];
        }
        else if(commandlets[i] == "&")
        {
            bg = true;
        }
        if(!in_rd && !out_rd && !bg) end++;
    }
    wait_flag = !bg;
    
    const char **argv = new const char *[end+1];
    for (int i = 0; i < end; i++) argv[i] = commandlets[i].c_str();
    argv[end] = NULL;

    // create the fork and execute the process
    if((child_pid = fork()) == -1)
    {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    else if(child_pid == 0)
    {
        if(out_rd) out_fd = open(out_file.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
        dup2(out_fd, 1);

        if(in_rd) in_fd = open(in_file.c_str(), O_RDONLY);
        dup2(in_fd, 0);

        execvp(commandlets[0].c_str(), (char **)argv);
        exit(0);
    }
    else
    {
        // wait for child process to complete
        if(wait_flag)
        {
            while(wait(NULL) != child_pid);

            if(in_fd != 0) close(in_fd);
            if(out_fd != 1) close(out_fd);
        }
        else
        {
            cout << "[BG] " << child_pid << endl;
        }
    }
    return;
}

int main(int argc, char** argv)
{
    string line;

    while(1)
    {
        cout << "\033[1;34m[group06] >> \033[0m";

        // get the command entered
        if(!getline(cin, line))
        {
            cout << endl;
            return 0;
        }
        if(line.size() == 0) continue;

        // seperate commands through pipe
        vector<string> commands;
        int start = 0, end = 0;
        while(end < line.size())
        {
            if(line[end] == '|')
            {
                commands.push_back(line.substr(start, end - start));
                start = end + 1;
            }
            end++;
        }
        if(start < line.size()) commands.push_back(line.substr(start, end - start));
        
        // generate pipes
        int num_pipes = commands.size() - 1;
        int **pipes = new int *[num_pipes];
        for(int j = 0; j < num_pipes; j++)
        {
            pipes[j] = new int[2];
            if(pipe(pipes[j]) == -1)
            {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }
        }

        // execute commands
        vector<string> commandlets;
        for(int i = 0; i < commands.size(); i++)
        {
            int in_fd = 0, out_fd = 1;
            if(i > 0) in_fd = pipes[i - 1][0];
            if(i < commands.size() - 1) out_fd = pipes[i][1];

            // remove the spaces and get each word in the command
            preprocess(commands[i], commandlets);
            run_command(commandlets, in_fd, out_fd);
            commandlets.clear();
        }

        fflush(stdin);
        fflush(stdout);
    }
    return 0;
}