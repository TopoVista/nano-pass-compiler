#pragma once

#include <iostream>
#include <vector>
#include <string>

using namespace std;

enum class IROp {
    Assign,
    Add, Sub, Mul, Div,
    Print,
    Return,
    Label,
    Jump,
    JumpIfFalse
};

struct IRInstr {
    IROp op;
    string dst;
    string src1;
    string src2;
};

struct IRPrinter {

    static void print(const vector<IRInstr>& ir) {
        cout << "\n===== IR DUMP =====\n";
        for (auto& i : ir)
            printInstr(i);
        cout << "===================\n";
    }

private:

    static void printInstr(const IRInstr& i) {
        switch (i.op) {

        case IROp::Assign:
            cout << i.dst << " = " << i.src1 << "\n";
            break;

        case IROp::Add:
            cout << i.dst << " = " << i.src1 << " + " << i.src2 << "\n";
            break;

        case IROp::Sub:
            cout << i.dst << " = " << i.src1 << " - " << i.src2 << "\n";
            break;

        case IROp::Mul:
            cout << i.dst << " = " << i.src1 << " * " << i.src2 << "\n";
            break;

        case IROp::Div:
            cout << i.dst << " = " << i.src1 << " / " << i.src2 << "\n";
            break;

        case IROp::Print:
            cout << "print " << i.src1 << "\n";
            break;

        case IROp::Return:
            cout << "return " << i.src1 << "\n";
            break;

        case IROp::Label:
            cout << i.dst << ":\n";
            break;

        case IROp::Jump:
            cout << "goto " << i.dst << "\n";
            break;

        case IROp::JumpIfFalse:
            cout << "ifFalse " << i.src1 << " goto " << i.dst << "\n";
            break;
        }
    }
};
