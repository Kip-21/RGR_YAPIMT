#include "interpreter.h"
#include <iostream>
#include <fstream>
#include <stack>
#include <stdexcept>
#include <cctype>
#include <climits>

using namespace std;

void executePoliz(const vector<string>& poliz, VarTable& varTable) {
    stack<string> runtimeStack;
    ifstream inFile("data.txt");
    ofstream errOut("output/errors.txt", ios::app);
    
    auto isNumeric = [](const string& s) {
        if (s.empty()) return false;
        size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        if (start == 1 && s.size() == 1) return false;
        for (size_t i = start; i < s.size(); ++i) {
            if (!isdigit((unsigned char)s[i]) && s[i] != '.') return false;
        }
        return true;
    };

    for (const string& token : poliz) {
        if (token == "АЭМ") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for АЭМ");
            string indexStr = runtimeStack.top(); runtimeStack.pop();
            string arrayName = runtimeStack.top(); runtimeStack.pop();
            
            int idx = 0;
            if (isNumeric(indexStr)) {
                idx = stoi(indexStr);
            } else {
                auto attr = varTable.getAttributes(indexStr);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized array index '" + indexStr + "'");
                idx = stoi(attr.value);
            }
            
            auto arrAttr = varTable.getAttributes(arrayName);
            if (arrAttr.array_size == 0) {
                cout << "\033[1;33m[RUNTIME WARNING] Array '" << arrayName << "' is empty or uninitialized. Returning default value 0.\033[0m\n";
                errOut << "[RUNTIME WARNING] Array '" << arrayName << "' is empty or uninitialized. Returning default value 0.\n";
                runtimeStack.push("0");
                continue;
            }
            if (idx < 0) {
                cout << "\033[1;33m[RUNTIME WARNING] Array index out of bounds (" << idx << ") for array '" << arrayName << "'. Correcting to 0.\033[0m\n";
                errOut << "[RUNTIME WARNING] Array index out of bounds (" << idx << ") for array '" << arrayName << "'. Correcting to 0.\n";
                idx = 0;
            } else if ((size_t)idx >= arrAttr.array_size) {
                cout << "\033[1;33m[RUNTIME WARNING] Array index out of bounds (" << idx << ") for array '" << arrayName << "'. Correcting to " << (arrAttr.array_size - 1) << ".\033[0m\n";
                errOut << "[RUNTIME WARNING] Array index out of bounds (" << idx << ") for array '" << arrayName << "'. Correcting to " << (arrAttr.array_size - 1) << ".\n";
                idx = arrAttr.array_size - 1;
            }
            
            runtimeStack.push(arrayName + "[" + to_string(idx) + "]");
        }
        else if (token == "ЧТЕНИЕ") {
            if (runtimeStack.empty()) throw runtime_error("Runtime Error: Stack underflow for ЧТЕНИЕ");
            string varName = runtimeStack.top(); runtimeStack.pop();
            string valStr;
            if (inFile >> valStr) {
                auto attr = varTable.getAttributes(varName);
                attr.value = valStr;
                varTable.update(varName, attr);
            } else {
                cout << "\033[1;33m[RUNTIME WARNING] Attempt to read beyond EOF from data.txt. Correcting by assigning 0 to '" << varName << "'.\033[0m\n";
                errOut << "[RUNTIME WARNING] Attempt to read beyond EOF from data.txt. Correcting by assigning 0 to '" << varName << "'.\n";
                auto attr = varTable.getAttributes(varName);
                attr.value = "0";
                varTable.update(varName, attr);
            }
        }
        else if (token == "+" || token == "-" || token == "*") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation " + token);
            string rightStr = runtimeStack.top(); runtimeStack.pop();
            string leftStr = runtimeStack.top(); runtimeStack.pop();
            
            auto resolveVal = [&](const string& s) -> double {
                if (isNumeric(s)) return stod(s);
                if (s.find('[') != string::npos) {
                    size_t bracketPos = s.find('[');
                    string arrName = s.substr(0, bracketPos);
                    int idx = stoi(s.substr(bracketPos + 1, s.size() - bracketPos - 2));
                    auto attr = varTable.getAttributes(arrName);
                    if (idx >= 0 && (size_t)idx < attr.array_values.size()) {
                        string val = attr.array_values[idx];
                        if (val == "-") throw runtime_error("Runtime Error: Uninitialized array element '" + s + "'");
                        return stod(val);
                    }
                    return 0;
                }
                auto attr = varTable.getAttributes(s);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized variable '" + s + "'");
                return stod(attr.value);
            };
            
            double r = resolveVal(rightStr);
            double l = resolveVal(leftStr);
            
            bool isInt = (leftStr.find('.') == string::npos && rightStr.find('.') == string::npos);
            if (!isNumeric(leftStr)) {
                if (varTable.getAttributes(leftStr).type == "int") isInt = true;
            }
            if (!isNumeric(rightStr)) {
                if (varTable.getAttributes(rightStr).type == "int") isInt = true;
            }

            if (isInt) {
                long long rl = (long long)r;
                long long ll = (long long)l;
                if (token == "+") {
                    if (rl > 0 && ll > INT_MAX - rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Overflow (+). Correcting to INT_MAX.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Overflow (+). Correcting to INT_MAX.\n";
                        runtimeStack.push(to_string(INT_MAX)); continue;
                    } else if (rl < 0 && ll < INT_MIN - rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Underflow (+). Correcting to INT_MIN.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Underflow (+). Correcting to INT_MIN.\n";
                        runtimeStack.push(to_string(INT_MIN)); continue;
                    }
                } else if (token == "*") {
                    if (rl > 0 && ll > 0 && ll > INT_MAX / rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Overflow (*). Correcting to INT_MAX.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Overflow (*). Correcting to INT_MAX.\n";
                        runtimeStack.push(to_string(INT_MAX)); continue;
                    } else if (rl < 0 && ll < 0 && ll < INT_MAX / rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Overflow (*). Correcting to INT_MAX.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Overflow (*). Correcting to INT_MAX.\n";
                        runtimeStack.push(to_string(INT_MAX)); continue;
                    } else if (rl > 0 && ll < 0 && ll < INT_MIN / rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Underflow (*). Correcting to INT_MIN.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Underflow (*). Correcting to INT_MIN.\n";
                        runtimeStack.push(to_string(INT_MIN)); continue;
                    } else if (rl < 0 && ll > 0 && ll > INT_MIN / rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Underflow (*). Correcting to INT_MIN.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Underflow (*). Correcting to INT_MIN.\n";
                        runtimeStack.push(to_string(INT_MIN)); continue;
                    }
                } else if (token == "-") {
                    if (rl < 0 && ll > INT_MAX + rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Overflow (-). Correcting to INT_MAX.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Overflow (-). Correcting to INT_MAX.\n";
                        runtimeStack.push(to_string(INT_MAX)); continue;
                    } else if (rl > 0 && ll < INT_MIN + rl) {
                        cout << "\033[1;33m[RUNTIME WARNING] Integer Underflow (-). Correcting to INT_MIN.\033[0m\n";
                        errOut << "[RUNTIME WARNING] Integer Underflow (-). Correcting to INT_MIN.\n";
                        runtimeStack.push(to_string(INT_MIN)); continue;
                    }
                }
            }

            double res = 0;
            if (token == "+") res = l + r;
            else if (token == "-") res = l - r;
            else if (token == "*") res = l * r;
            
            if (isInt) runtimeStack.push(to_string((int)res));
            else runtimeStack.push(to_string(res));
        }
        else if (token == "<" || token == ">" || token == "==" || token == "!=") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation " + token);
            string rightStr = runtimeStack.top(); runtimeStack.pop();
            string leftStr = runtimeStack.top(); runtimeStack.pop();
            
            auto resolveVal = [&](const string& s) -> double {
                if (isNumeric(s)) return stod(s);
                if (s.find('[') != string::npos) {
                    size_t bracketPos = s.find('[');
                    string arrName = s.substr(0, bracketPos);
                    int idx = stoi(s.substr(bracketPos + 1, s.size() - bracketPos - 2));
                    auto attr = varTable.getAttributes(arrName);
                    if (idx >= 0 && (size_t)idx < attr.array_values.size()) {
                        string val = attr.array_values[idx];
                        if (val == "-") throw runtime_error("Runtime Error: Uninitialized array element '" + s + "'");
                        return stod(val);
                    }
                    return 0;
                }
                auto attr = varTable.getAttributes(s);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized variable '" + s + "'");
                return stod(attr.value);
            };
            
            double r = resolveVal(rightStr);
            double l = resolveVal(leftStr);
            
            bool res = false;
            if (token == "<") res = l < r;
            else if (token == ">") res = l > r;
            else if (token == "==") res = l == r;
            else if (token == "!=") res = l != r;
            
            runtimeStack.push(res ? "1" : "0");
        }
        else if (token == "=") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation =");
            string valStr = runtimeStack.top(); runtimeStack.pop();
            string varName = runtimeStack.top(); runtimeStack.pop();
            
            string resolvedVal = valStr;
            if (!isNumeric(valStr)) {
                if (valStr.find('[') != string::npos) {
                    size_t bracketPos = valStr.find('[');
                    string arrName = valStr.substr(0, bracketPos);
                    int idx = stoi(valStr.substr(bracketPos + 1, valStr.size() - bracketPos - 2));
                    auto attr = varTable.getAttributes(arrName);
                    if (idx >= 0 && (size_t)idx < attr.array_values.size()) {
                        resolvedVal = attr.array_values[idx];
                    } else {
                        resolvedVal = "0";
                    }
                } else if (varTable.contains(valStr)) {
                    resolvedVal = varTable.getAttributes(valStr).value;
                }
            }
            
            if (varName.find('[') != string::npos) {
                size_t bracketPos = varName.find('[');
                string arrName = varName.substr(0, bracketPos);
                int idx = stoi(varName.substr(bracketPos + 1, varName.size() - bracketPos - 2));
                auto attr = varTable.getAttributes(arrName);
                if ((size_t)idx >= attr.array_values.size()) attr.array_values.resize(idx + 1, "0");
                attr.array_values[idx] = resolvedVal;
                
                string newVal = "[";
                for (size_t i = 0; i < attr.array_values.size(); ++i) {
                    newVal += attr.array_values[i];
                    if (i < attr.array_values.size() - 1) newVal += ", ";
                }
                newVal += "]";
                attr.value = newVal;
                
                varTable.update(arrName, attr);
            } else {
                auto attr = varTable.getAttributes(varName);
                attr.value = resolvedVal;
                varTable.update(varName, attr);
            }
        }
        else {
            runtimeStack.push(token);
        }
    }
    
    cout << "\n=== VM Execution Finished ===\n";
    if (!runtimeStack.empty()) {
        cout << "Stack top: " << runtimeStack.top() << "\n";
    }
}
