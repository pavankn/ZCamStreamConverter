#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>

class NativeProcess {
public:
    NativeProcess(const std::string& cmdLine) {
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = { 0 };

        // We need a mutable buffer for CreateProcessA
        std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back('\0');

        if (!CreateProcessA(NULL, mutableCmd.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            throw std::runtime_error("Failed to launch: " + std::to_string(GetLastError()));
        }

        hProcess = pi.hProcess;
        processId = pi.dwProcessId;
        CloseHandle(pi.hThread); // We don't need the thread handle
    }

    ~NativeProcess() {
        if (hProcess != NULL) {
            CloseHandle(hProcess);
        }
    }

    NativeProcess& operator=(NativeProcess&& other) noexcept {
        if (this != &other) {
            if (hProcess)
                CloseHandle(hProcess);

            hProcess = other.hProcess;
            processId = other.processId;

            other.hProcess = NULL;
        }
        return *this;
    }

    DWORD wait() {
        WaitForSingleObject(hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(hProcess, &exitCode);
        return exitCode;
    }

    // Prevent copying (don't want two objects closing the same handle)
    NativeProcess(const NativeProcess&) = delete;
    NativeProcess& operator=(const NativeProcess&) = delete;

    // Allow moving
    NativeProcess(NativeProcess&& other) noexcept : hProcess(other.hProcess), processId(other.processId) {
        other.hProcess = NULL;
    }

    HANDLE get() const { return hProcess; }
    DWORD id() const { return processId; }

private:
    HANDLE hProcess = NULL;
    DWORD processId = 0;
};