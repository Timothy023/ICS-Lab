#include "policy.h"
#include <bits/stdc++.h>
using namespace std;

int maxn = 0;

namespace XuanXue {

    int sumtp = 0;
    int sum = 0;
    bool flag = false;
    Event::Task ccpu, cio;
    bool ucpu = false;
    int id[200];

    struct cmp{
        bool operator () (Event::Task &i, Event::Task &j) {
            return i.deadline - i.arrivalTime > j.deadline - j.arrivalTime;
        }
    };

    priority_queue < Event::Task, vector <Event::Task>, cmp > que_cpu, que_io;
    priority_queue < Event::Task, vector <Event::Task>, cmp > que_cpu_ddl, que_io_ddl;

    Action policy(const std::vector<Event>& events, int current_cpu, int current_io) {
        
        int ti = 0;
        bool isFinish = false;
        for (auto e : events) {
            ti = e.time;
            if (e.type == Event::Type::kTimer) {
                continue;
            }
            else if (e.type == Event::Type::kTaskArrival) {
                que_cpu.push(e.task);
            }
            else if (e.type == Event::Type::kTaskFinish) {
                isFinish = true;
                sum++;
            }
            else if (e.type == Event::Type::kIoRequest) {
                isFinish = true;
                que_io.push(ccpu);
            }
            else if (e.type == Event::Type::kIoEnd) {
                que_cpu.push(cio);
                continue;
            }
        }

        if (!isFinish && ucpu) {
            que_cpu.push(ccpu);
        }
        else {
            ucpu = false;
        }

        int x = 0, y = current_io;

        while (!que_cpu.empty()) {
            if (que_cpu.top().deadline <= ti) {
                Event::Task e = que_cpu.top();
                que_cpu.pop();
                que_cpu_ddl.push(e);
            }
            else break;
        }
        if (!que_cpu.empty()) {
            ccpu = que_cpu.top();
            que_cpu.pop();
            x = ccpu.taskId;
            ucpu = true;
        }
        else if (!que_cpu_ddl.empty()) {
            ccpu = que_cpu_ddl.top();
            que_cpu_ddl.pop();
            x = ccpu.taskId;
            ucpu = true;
        }

        while (!que_io.empty()) {
            if (que_io.top().deadline <= ti) {
                Event::Task e = que_io.top();
                que_io.pop();
                que_io_ddl.push(e);
            }
            else break;
        }
        if (current_io == 0) {
            if (!que_io.empty()) {
                cio = que_io.top();
                que_io.pop();
                y = cio.taskId;
            }
            else if (!que_io_ddl.empty() && sum >= maxn) {
                cio = que_io_ddl.top();
                que_io_ddl.pop();
                y = cio.taskId;
            }
        }
        return Action{x, y};
    }
}

Action policy(const std::vector<Event>& events, int current_cpu, int current_io) {
    maxn = 15;
    return XuanXue::policy(events, current_cpu, current_io);
}