#include <stdio.h>
#include <string>
#include <sstream>
#include <iostream>
#include <chrono>
#include <map>
#include "ppcemumain.h"
#include "ppcmemory.h"


using namespace std;

void show_help()
{
    cout << "Debugger commands:" << endl;
    cout << "  step      -- execute single instruction" << endl;
    cout << "  until X   -- execute until address X is reached" << endl;
    cout << "  regs      -- dump content of the GRPs" << endl;
    cout << "  disas X,n -- disassemble N instructions starting at address X" << endl;
    cout << "  quit      -- quit the debugger" << endl << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

void dump_regs()
{
    for (uint32_t i = 0; i < 32; i++)
        cout << "GPR " << dec << i << " : " << hex << ppc_state.ppc_gpr[i] << endl;

    cout << "PC: "  << hex << ppc_state.ppc_pc     << endl;
    cout << "LR: "  << hex << ppc_state.ppc_spr[8] << endl;
    cout << "CR: "  << hex << ppc_state.ppc_cr     << endl;
    cout << "CTR: " << hex << ppc_state.ppc_spr[9] << endl;
    cout << "XER: " << hex << ppc_state.ppc_spr[1] << endl;
    cout << "MSR: " << hex << ppc_state.ppc_msr    << endl;
}

void execute_single_instr()
{
    quickinstruction_translate(ppc_state.ppc_pc);
    //cout << "Current instruction: " << hex << ppc_cur_instruction << endl;
    ppc_main_opcode();
    if (grab_branch && !grab_exception) {
        //cout << "Grab branch, EA: " << hex << ppc_next_instruction_address << endl;
        ppc_state.ppc_pc = ppc_next_instruction_address;
        grab_branch = 0;
        ppc_tbr_update();
    }
    else if (grab_return || grab_exception) {
        ppc_state.ppc_pc = ppc_next_instruction_address;
        grab_exception = 0;
        grab_return = 0;
        ppc_tbr_update();
    }
    else {
        ppc_state.ppc_pc += 4;
        ppc_tbr_update();
    }
}

void execute_optimized()
{
    uint32_t cache[25];
    uint32_t save_pc, fake_pc, ctr;
    string   cmd;

    cout << "Executing optimized loop..." << endl;

    fake_pc = ppc_state.ppc_pc;

    for(int i = 0; i < 21; i++) {
        quickinstruction_translate(ppc_state.ppc_pc);
        cache[i] = ppc_cur_instruction;
        ppc_state.ppc_pc += 4;
        ppc_cur_instruction = 0;
    }

    save_pc = ppc_state.ppc_pc;
    ctr = ppc_state.ppc_spr[9];

    while (fake_pc < save_pc) {
        ppc_cur_instruction = cache[(fake_pc - 0xfff03454) >> 2];
        if (ppc_cur_instruction == 0x4200FFB0) { // emulate bdnz directly
            if (--ctr) {
                ppc_state.ppc_spr[9] = ctr;
                fake_pc -= 0x50;
            } else {
                fake_pc += 4;
            }
        } else {
            ppc_main_opcode();
            fake_pc += 4;
        }
    }

    ppc_state.ppc_pc = save_pc; // FEXME: hardcoded
    ppc_cur_instruction = 0;
    //dump_regs();
}

void benchmark_emulator()
{
    int instr_index;
    uint32_t ctr;

    uint32_t code[] = {
        0x38800400, // li r4, 0x400
        0x7C8903A6, // mtctr r4
        0x38A00000, // li r5, 0
        0x38A50001, // addi r5, r5, 1
        0x38A50002, // addi r5, r5, 2
        0x38A50003, // addi r5, r5, 3
        0x38A50004, // addi r5, r5, 4
        0x38A50005, // addi r5, r5, 5
        0x38A50006, // addi r5, r5, 6
        0x38A50007, // addi r5, r5, 7
        0x38A50008, // addi r5, r5, 8
        0x4200FFE0, // bdnz *pc-32
    };

    cout << "HiRes clock resolution: " <<
		chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::duration(1)).count()
		<< " ns" << endl;

    // run the clock once for cache fill etc.
    auto start_time = chrono::steady_clock::now();
    auto end_time = chrono::steady_clock::now();
    auto time_elapsed = chrono::duration_cast<chrono::nanoseconds>(end_time - start_time);
    cout << "Time elapsed (dry run): " << time_elapsed.count() << " ns" << endl;

    start_time = chrono::steady_clock::now();

    instr_index = 0;

    while (1) {
        ppc_cur_instruction = code[instr_index];
        if (ppc_cur_instruction == 0x4200FFE0) { // emulate bdnz directly
            ctr = ppc_state.ppc_spr[9];
            if (--ctr) {
                ppc_state.ppc_spr[9] = ctr;
                instr_index = 3;
            } else {
                ppc_state.ppc_spr[9] = ctr;
                break;
            }
        } else {
            ppc_main_opcode();
            //ppc_tbr_update();
            instr_index++;
        }
    }

    end_time = chrono::steady_clock::now();
    time_elapsed = chrono::duration_cast<chrono::nanoseconds>(end_time - start_time);
    cout << "Time elapsed: " << time_elapsed.count() << " ns" << endl;
    //dump_regs();
}

void execute_until(uint32_t goal_addr)
{
    while(ppc_state.ppc_pc != goal_addr) {
        if (ppc_state.ppc_pc >= 0xfff03454 && ppc_state.ppc_pc < 0xfff034a8 &&
            (goal_addr < 0xfff03454 || goal_addr >= 0xfff034a8))
            execute_optimized();
        else
            execute_single_instr();
    }
}

void enter_debugger()
{
    string inp, cmd, addr_str, last_cmd;
    uint32_t addr;
    std::stringstream ss;

    cout << "Welcome to the PowerPC debugger." << endl;
    cout << "Please enter a command or 'help'." << endl << endl;

    while (1) {
        cout << "ppcdbg> ";

        /* reset string stream */
        ss.str("");
        ss.clear();

        cmd = "";
        getline(cin, inp, '\n');
        ss.str(inp);
        ss >> cmd;

        if (cmd.empty() && !last_cmd.empty()) {
            cmd = last_cmd;
            cout << cmd << endl;
        }
        if (cmd == "help") {
            show_help();
        } else if (cmd == "quit") {
            break;
        } else if (cmd == "regs") {
            dump_regs();
        } else if (cmd == "step") {
            execute_single_instr();
        } else if (cmd == "until") {
            chrono::time_point<chrono::system_clock> start, end;
            start = chrono::system_clock::now();
            ss >> addr_str;
            addr = stol(addr_str, NULL, 16);
            execute_until(addr);
            end = chrono::system_clock::now();
            int elapsed_seconds = chrono::duration_cast<chrono::seconds>(end-start).count();
            cout << "Ready in " << elapsed_seconds << "s" << endl;
        } else if (cmd == "disas") {
            cout << "Disassembling not implemented yet. Sorry!" << endl;
        } else if (cmd == "benchmark") {
            benchmark_emulator();
        } else {
            cout << "Unknown command: " << cmd << endl;
            continue;
        }
        last_cmd = cmd;
    }
}
