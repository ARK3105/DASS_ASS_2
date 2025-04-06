// DailyFoodLog implementation
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <memory>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <functional>
#include <limits>

#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Forward declaration
class Food;
class FoodDatabaseManager;

// LogEntry class to represent a food entry in the daily log
struct LogEntry {
    string foodName;
    float servings;
    time_t timestamp;

    LogEntry(const string& name, float amount, time_t time = time(nullptr))
        : foodName(name), servings(amount), timestamp(time) {}
    
    json toJson() const {
        json j;
        j["foodName"] = foodName;
        j["servings"] = servings;
        j["timestamp"] = timestamp;
        return j;
    }
    
    static LogEntry fromJson(const json& j) {
        return LogEntry(j["foodName"], j["servings"], j["timestamp"]);
    }
};

// Command pattern for undo functionality
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual string getDescription() const = 0;
};

// Daily Food Log class
class DailyFoodLog {
private:
    map<string, vector<LogEntry>> dailyLogs; // date -> log entries
    stack<unique_ptr<Command>> commandHistory;
    string currentDate;
    string logFilePath;
    FoodDatabaseManager& dbManager;
    bool modified;

    // Get today's date as string in YYYY-MM-DD format
    string getTodayDate() const {
        time_t now = time(nullptr);
        tm* localTime = localtime(&now);
        
        stringstream ss;
        ss << setw(4) << setfill('0') << (localTime->tm_year + 1900) << "-"
           << setw(2) << setfill('0') << (localTime->tm_mon + 1) << "-"
           << setw(2) << setfill('0') << localTime->tm_mday;
        return ss.str();
    }

    // Validate date format (YYYY-MM-DD)
    bool isValidDateFormat(const string& date) const {
        if (date.length() != 10) return false;
        if (date[4] != '-' || date[7] != '-') return false;
        
        // Check if all other characters are digits
        for (size_t i = 0; i < date.length(); i++) {
            if (i != 4 && i != 7 && !isdigit(date[i])) {
                return false;
            }
        }
        
        return true;
    }

public:
    DailyFoodLog(FoodDatabaseManager& dbm, const string& filePath = "food_logs.json")
        : dbManager(dbm), logFilePath(filePath), modified(false) {
        currentDate = getTodayDate();
    }
    
    ~DailyFoodLog() {
        // Command history will be cleared automatically
    }
    
    bool loadLogs() {
        dailyLogs.clear();
        
        ifstream file(logFilePath);
        if (!file.is_open()) {
            cout << "No existing logs found. Starting with empty logs." << endl;
            return false;
        }
        
        try {
            json j;
            file >> j;
            
            for (const auto& [date, entries] : j.items()) {
                vector<LogEntry> dayEntries;
                for (const auto& entry : entries) {
                    dayEntries.push_back(LogEntry::fromJson(entry));
                }
                dailyLogs[date] = dayEntries;
            }
            
            cout << "Logs loaded from " << logFilePath << endl;
            return true;
        } catch (const exception& e) {
            cout << "Error loading logs: " << e.what() << endl;
            return false;
        }
    }
    
    bool saveLogs() {
        try {
            json j;
            
            for (const auto& [date, entries] : dailyLogs) {
                json dateEntries = json::array();
                for (const auto& entry : entries) {
                    dateEntries.push_back(entry.toJson());
                }
                j[date] = dateEntries;
            }
            
            ofstream file(logFilePath);
            if (!file.is_open()) {
                cout << "Error: Unable to open log file for writing." << endl;
                return false;
            }
            
            file << j.dump(4);  // Pretty print with 4 spaces
            file.close();
            
            modified = false;
            cout << "Logs saved to " << logFilePath << endl;
            return true;
        } catch (const exception& e) {
            cout << "Error saving logs: " << e.what() << endl;
            return false;
        }
    }
    
    void setCurrentDate(const string& date) {
        if (isValidDateFormat(date)) {
            currentDate = date;
            cout << "Current date set to: " << currentDate << endl;
        } else {
            cout << "Invalid date format. Please use YYYY-MM-DD format." << endl;
        }
    }
    
    const string& getCurrentDate() const {
        return currentDate;
    }
    
    // Command for adding food to log
    class AddFoodCommand : public Command {
    private:
        DailyFoodLog& log;
        string date;
        LogEntry entry;
        
    public:
        AddFoodCommand(DailyFoodLog& log, const string& date, const LogEntry& entry)
            : log(log), date(date), entry(entry) {}
        
        void execute() override {
            log.dailyLogs[date].push_back(entry);
            log.modified = true;
        }
        
        void undo() override {
            if (!log.dailyLogs[date].empty()) {
                log.dailyLogs[date].pop_back();
                if (log.dailyLogs[date].empty()) {
                    log.dailyLogs.erase(date);
                }
                log.modified = true;
            }
        }
        
        string getDescription() const override {
            stringstream ss;
            ss << "Add " << entry.servings << " serving(s) of '" << entry.foodName << "' on " << date;
            return ss.str();
        }
    };
    
    // Command for removing food from log
    class RemoveFoodCommand : public Command {
    private:
        DailyFoodLog& log;
        string date;
        size_t index;
        LogEntry removedEntry;
        
    public:
        RemoveFoodCommand(DailyFoodLog& log, const string& date, size_t idx)
            : log(log), date(date), index(idx) {
            if (log.dailyLogs.find(date) != log.dailyLogs.end() && index < log.dailyLogs[date].size()) {
                removedEntry = log.dailyLogs[date][index];
            }
        }
        
        void execute() override {
            if (log.dailyLogs.find(date) != log.dailyLogs.end() && index < log.dailyLogs[date].size()) {
                log.dailyLogs[date].erase(log.dailyLogs[date].begin() + index);
                if (log.dailyLogs[date].empty()) {
                    log.dailyLogs.erase(date);
                }
                log.modified = true;
            }
        }
        
        void undo() override {
            log.dailyLogs[date].insert(log.dailyLogs[date].begin() + index, removedEntry);
            log.modified = true;
        }
        
        string getDescription() const override {
            stringstream ss;
            ss << "Remove " << removedEntry.servings << " serving(s) of '" 
               << removedEntry.foodName << "' from " << date;
            return ss.str();
        }
    };
    
    void addFoodToLog(const string& foodName, float servings) {
        LogEntry entry(foodName, servings);
        auto cmd = make_unique<AddFoodCommand>(*this, currentDate, entry);
        cmd->execute();
        commandHistory.push(move(cmd));
        
        float calories = calculateEntryCalories(foodName, servings);
        cout << "Added " << servings << " serving(s) of '" << foodName 
             << "' (" << calories << " calories) to log for " << currentDate << endl;
    }
    
    void removeFoodFromLog(size_t index) {
        if (dailyLogs.find(currentDate) == dailyLogs.end() || 
            index >= dailyLogs[currentDate].size()) {
            cout << "Invalid entry index." << endl;
            return;
        }
        
        auto cmd = make_unique<RemoveFoodCommand>(*this, currentDate, index);
        string foodName = dailyLogs[currentDate][index].foodName;
        float servings = dailyLogs[currentDate][index].servings;
        
        cmd->execute();
        commandHistory.push(move(cmd));
        
        cout << "Removed " << servings << " serving(s) of '" << foodName 
             << "' from log for " << currentDate << endl;
    }
    
    void undoLastCommand() {
        if (commandHistory.empty()) {
            cout << "Nothing to undo." << endl;
            return;
        }
        
        auto& cmd = commandHistory.top();
        cout << "Undoing: " << cmd->getDescription() << endl;
        cmd->undo();
        commandHistory.pop();
    }
    
    void displayLogForDate(const string& date) const {
        auto it = dailyLogs.find(date);
        if (it == dailyLogs.end() || it->second.empty()) {
            cout << "No log entries for " << date << "." << endl;
            return;
        }
        
        float totalCalories = 0.0f;
        
        cout << "\n=== Food Log for " << date << " ===" << endl;
        cout << setw(4) << "#" << setw(25) << "Food" << setw(12) << "Servings" 
             << setw(12) << "Calories" << endl;
        cout << string(53, '-') << endl;
        
        for (size_t i = 0; i < it->second.size(); ++i) {
            const auto& entry = it->second[i];
            float calories = calculateEntryCalories(entry.foodName, entry.servings);
            totalCalories += calories;
            
            cout << setw(4) << i+1 << setw(25) << truncateString(entry.foodName, 24) 
                 << setw(12) << entry.servings << setw(12) << calories << endl;
        }
        
        cout << string(53, '-') << endl;
        cout << setw(41) << "Total Calories:" << setw(12) << totalCalories << endl;
        cout << "===========================" << endl;
    }
    
    void displayCurrentLog() const {
        displayLogForDate(currentDate);
    }
    
    void displayAllDates() const {
        if (dailyLogs.empty()) {
            cout << "No log entries available." << endl;
            return;
        }
        
        cout << "\n=== Available Log Dates ===" << endl;
        for (const auto& [date, entries] : dailyLogs) {
            float totalCalories = 0.0f;
            for (const auto& entry : entries) {
                totalCalories += calculateEntryCalories(entry.foodName, entry.servings);
            }
            
            cout << date << " - " << entries.size() << " entries, " 
                 << totalCalories << " total calories" << endl;
        }
        cout << "===========================" << endl;
    }
    
    float calculateEntryCalories(const string& foodName, float servings) const;
    
    bool isModified() const {
        return modified;
    }
    
    size_t getCommandHistorySize() const {
        return commandHistory.size();
    }
    
private:
    string truncateString(const string& str, size_t length) const {
        if (str.length() <= length) return str;
        return str.substr(0, length - 3) + "...";
    }
};

// Implementation of the CLI for daily logs
class DietAssistantCLI {
private:
    FoodDatabaseManager& dbManager;
    DailyFoodLog foodLog;
    bool running;
    
    // Enhanced menu with log functions
    void displayMenu() {
        cout << "\n===== Diet Assistant Menu =====\n";
        cout << "1. Search foods\n";
        cout << "2. View food details\n";
        cout << "3. Add basic food\n";
        cout << "4. Create composite food\n";
        cout << "5. List all foods\n";
        cout << "6. Save database\n";
        cout << "7. View current food log\n";  // New option
        cout << "8. Add food to current log\n"; // New option
        cout << "9. Remove food from log\n";    // New option
        cout << "10. Change current date\n";    // New option
        cout << "11. View logs for another date\n"; // New option
        cout << "12. View all log dates\n";     // New option
        cout << "13. Undo last action\n";       // New option
        cout << "14. Save logs\n";              // New option
        cout << "15. Exit\n";                   // Changed number
        cout << "==============================\n";
        cout << "Enter choice (1-15): ";
    }
    
    // Existing methods (searchFoods, viewFoodDetails, etc.)
    // ...

    // New methods for log functionality
    void viewCurrentLog() {
        cout << "\nCurrent date: " << foodLog.getCurrentDate() << endl;
        foodLog.displayCurrentLog();
    }
    
    void addFoodToLog() {
        cout << "\n=== Add Food to Log (" << foodLog.getCurrentDate() << ") ===" << endl;
        
        // First, let the user search for a food
        cout << "Search for food (or press Enter to list all): ";
        string query;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, query);
        
        vector<shared_ptr<Food>> results;
        if (query.empty()) {
            // List all foods instead
            listAllFoods();
            cout << "\nEnter food name: ";
            getline(cin, query);
            shared_ptr<Food> food = dbManager.getFood(query);
            if (food) results.push_back(food);
        } else {
            // Perform search
            results = searchFoodWithKeywords(query);
        }
        
        if (results.empty()) {
            cout << "No foods found." << endl;
            return;
        }
        
        // If there's only one result, use it directly
        shared_ptr<Food> selectedFood;
        if (results.size() == 1) {
            selectedFood = results[0];
        } else {
            // Display search results
            cout << "\n=== Search Results (" << results.size() << " found) ===" << endl;
            for (size_t i = 0; i < results.size(); ++i) {
                cout << i+1 << ". " << results[i]->getName() 
                     << " (" << results[i]->getType() << ") - " 
                     << results[i]->getCalories() << " calories" << endl;
            }
            
            // Let the user select a food
            int selection;
            cout << "\nSelect food (1-" << results.size() << ") or 0 to cancel: ";
            cin >> selection;
            
            if (selection <= 0 || selection > static_cast<int>(results.size())) {
                cout << "Selection cancelled." << endl;
                return;
            }
            
            selectedFood = results[selection - 1];
        }
        
        // Ask for number of servings
        float servings;
        cout << "Enter number of servings: ";
        cin >> servings;
        
        if (servings <= 0) {
            cout << "Invalid number of servings." << endl;
            return;
        }
        
        // Add to log
        foodLog.addFoodToLog(selectedFood->getName(), servings);
    }
    
    vector<shared_ptr<Food>> searchFoodWithKeywords(const string& query) {
        // First check if we need to parse multiple keywords
        if (query.find(',') == string::npos) {
            // Single keyword or name search
            return dbManager.searchFoods(query);
        }
        
        // Parse comma-separated keywords
        vector<string> keywords;
        size_t pos = 0;
        string token;
        string queryCopy = query;
        
        while ((pos = queryCopy.find(',')) != string::npos) {
            token = queryCopy.substr(0, pos);
            token.erase(0, token.find_first_not_of(' '));
            token.erase(token.find_last_not_of(' ') + 1);
            if (!token.empty()) keywords.push_back(token);
            queryCopy.erase(0, pos + 1);
        }
        
        // Add the last keyword
        queryCopy.erase(0, queryCopy.find_first_not_of(' '));
        queryCopy.erase(queryCopy.find_last_not_of(' ') + 1);
        if (!queryCopy.empty()) keywords.push_back(queryCopy);
        
        if (keywords.empty()) {
            return {};
        }
        
        // Ask if they want to match ANY or ALL keywords
        char matchChoice;
        cout << "Match (A)ll keywords or (O)ne or more? [A/O]: ";
        cin >> matchChoice;
        
        vector<shared_ptr<Food>> results;
        set<string> foundFoods; // To track unique foods
        
        for (const auto& keyword : keywords) {
            auto keywordResults = dbManager.searchFoods(keyword);
            
            for (const auto& food : keywordResults) {
                if (matchChoice == 'A' || matchChoice == 'a') {
                    // For "ALL", we'll collect foods and check later
                    foundFoods.insert(food->getName());
                } else {
                    // For "ANY", add if not already in results
                    if (find_if(results.begin(), results.end(), 
                        [&food](const shared_ptr<Food>& f) { 
                            return f->getName() == food->getName(); 
                        }) == results.end()) {
                        results.push_back(food);
                    }
                }
            }
        }
        
        if (matchChoice == 'A' || matchChoice == 'a') {
            // For "ALL", we need to check which foods matched all keywords
            for (const auto& foodName : foundFoods) {
                shared_ptr<Food> food = dbManager.getFood(foodName);
                if (!food) continue;
                
                bool matchesAll = true;
                for (const auto& keyword : keywords) {
                    bool matchesKeyword = false;
                    string lowerFoodName = food->getName();
                    transform(lowerFoodName.begin(), lowerFoodName.end(), lowerFoodName.begin(), ::tolower);
                    
                    string lowerKeyword = keyword;
                    transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
                    
                    if (lowerFoodName.find(lowerKeyword) != string::npos) {
                        matchesKeyword = true;
                    } else {
                        // Check if matches any of the food's keywords
                        for (const auto& foodKeyword : food->getKeywords()) {
                            string lowerFoodKeyword = foodKeyword;
                            transform(lowerFoodKeyword.begin(), lowerFoodKeyword.end(), 
                                     lowerFoodKeyword.begin(), ::tolower);
                            
                            if (lowerFoodKeyword.find(lowerKeyword) != string::npos) {
                                matchesKeyword = true;
                                break;
                            }
                        }
                    }
                    
                    if (!matchesKeyword) {
                        matchesAll = false;
                        break;
                    }
                }
                
                if (matchesAll) {
                    results.push_back(food);
                }
            }
        }
        
        return results;
    }
    
    void removeFoodFromLog() {
        cout << "\n=== Remove Food from Log (" << foodLog.getCurrentDate() << ") ===" << endl;
        
        // Display current log first
        foodLog.displayCurrentLog();
        
        cout << "Enter entry number to remove (or 0 to cancel): ";
        int selection;
        cin >> selection;
        
        if (selection <= 0) {
            cout << "Removal cancelled." << endl;
            return;
        }
        
        // Remove the selected entry (adjust index to 0-based)
        foodLog.removeFoodFromLog(selection - 1);
    }
    
    void changeCurrentDate() {
        cout << "\n=== Change Current Date ===" << endl;
        cout << "Current date: " << foodLog.getCurrentDate() << endl;
        cout << "Enter new date (YYYY-MM-DD) or 'today' for today: ";
        
        string date;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, date);
        
        if (date == "today") {
            date = foodLog.getTodayDate();
        }
        
        foodLog.setCurrentDate(date);
    }
    
    void viewLogForDate() {
        cout << "\n=== View Log for Date ===" << endl;
        cout << "Enter date (YYYY-MM-DD): ";
        
        string date;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, date);
        
        foodLog.displayLogForDate(date);
    }
    
    void viewAllLogDates() {
        foodLog.displayAllDates();
    }
    
    void undoLastAction() {
        foodLog.undoLastCommand();
    }
    
    void saveLogs() {
        foodLog.saveLogs();
    }
    
    void handleExit() {
        bool needToSave = false;
        
        if (dbManager.isModified()) {
            cout << "Database has unsaved changes. Save before exit? (y/n): ";
            char choice;
            cin >> choice;
            
            if (choice == 'y' || choice == 'Y') {
                dbManager.saveDatabase();
            }
        }
        
        if (foodLog.isModified()) {
            cout << "Food logs have unsaved changes. Save before exit? (y/n): ";
            char choice;
            cin >> choice;
            
            if (choice == 'y' || choice == 'Y') {
                foodLog.saveLogs();
            }
        }
        
        running = false;
    }

public:
    DietAssistantCLI(FoodDatabaseManager& dbm, const string& databasePath = "food_database.json",
                   const string& logPath = "food_logs.json") 
        : dbManager(dbm), foodLog(dbm, logPath), running(false) {}
    
    void start() {
        running = true;
        
        // Load database and logs
        dbManager.loadDatabase();
        foodLog.loadLogs();
        
        cout << "Welcome to Diet Assistant!" << endl;
        cout << "Current date: " << foodLog.getCurrentDate() << endl;
        
        while (running) {
            displayMenu();
            
            int choice;
            cin >> choice;
            
            switch (choice) {
                // Existing options
                case 1: searchFoods(); break;
                case 2: viewFoodDetails(); break;
                case 3: addBasicFood(); break;
                case 4: createCompositeFood(); break;
                case 5: listAllFoods(); break;
                case 6: dbManager.saveDatabase(); break;
                
                // New log options
                case 7: viewCurrentLog(); break;
                case 8: addFoodToLog(); break;
                case 9: removeFoodFromLog(); break;
                case 10: changeCurrentDate(); break;
                case 11: viewLogForDate(); break;
                case 12: viewAllLogDates(); break;
                case 13: undoLastAction(); break;
                case 14: saveLogs(); break;
                case 15: handleExit(); break;
                
                default:
                    cout << "Invalid choice. Please try again." << endl;
            }
        }
        
        cout << "Thank you for using Diet Assistant. Goodbye!" << endl;
    }
};

// Implement the calculateEntryCalories function that depends on FoodDatabaseManager
float DailyFoodLog::calculateEntryCalories(const string& foodName, float servings) const {
    shared_ptr<Food> food = dbManager.getFood(foodName);
    if (food) {
        return food->getCalories() * servings;
    }
    return 0.0f;
}

// Modified main function to use the new implementation
int main() {
    FoodDatabaseManager dbManager("food_database.json");
    DietAssistantCLI dietAssistant(dbManager, "food_database.json", "food_logs.json");
    dietAssistant.start();
    return 0;
}