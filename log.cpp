#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <ctime>
#include <stack>
#include <memory>
#include <set>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "json.hpp"

using json = nlohmann::json;

// Forward declarations
class Command;
class FoodEntry;

// Basic data structures
struct Food {
    std::string name;
    std::string type;  // "basic" or "composite"
    double calories;
    std::vector<std::string> keywords;
    std::map<std::string, double> components;  // For composite foods: component name -> servings

    Food() = default;
    
    Food(const json& j) {
        name = j["name"];
        type = j["type"];
        calories = j["calories"];
        
        for (const auto& keyword : j["keywords"]) {
            keywords.push_back(keyword);
        }
        
        if (type == "composite" && j.contains("components")) {
            for (const auto& comp : j["components"]) {
                components[comp["name"]] = comp["servings"];
            }
        }
    }
};

// Food log entry for a specific day
class FoodEntry {
public:
    std::string foodName;
    double servings;
    double calories;

    FoodEntry(const std::string& name, double servs, double cals) 
        : foodName(name), servings(servs), calories(cals) {}
};

// Date handling utility
class DateUtil {
public:
    static std::string getCurrentDate() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d");
        return ss.str();
    }
    
    static bool isValidDate(const std::string& dateStr) {
        if (dateStr.length() != 10) return false;
        
        // Check format: YYYY-MM-DD
        for (int i = 0; i < 10; i++) {
            if ((i == 4 || i == 7) && dateStr[i] != '-') return false;
            else if (i != 4 && i != 7 && !std::isdigit(dateStr[i])) return false;
        }
        
        int year = std::stoi(dateStr.substr(0, 4));
        int month = std::stoi(dateStr.substr(5, 2));
        int day = std::stoi(dateStr.substr(8, 2));
        
        if (month < 1 || month > 12) return false;
        
        int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
        // Adjust for leap year
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
            daysInMonth[2] = 29;
        }
        
        return day >= 1 && day <= daysInMonth[month];
    }
};

// Command interface for undo functionality
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string getDescription() const = 0;
};

// Food diary main class
class FoodDiary {
private:
    std::string databaseFile; //
    std::string logFile;
    std::map<std::string, Food> foods; //
    std::map<std::string, std::vector<FoodEntry>> dailyLogs;
    std::stack<std::shared_ptr<Command>> undoStack;
    std::string currentDate;

public:
    FoodDiary(const std::string& dbFile, const std::string& log) 
        : databaseFile(dbFile), logFile(log), currentDate(DateUtil::getCurrentDate()) {
        loadDatabase();
        loadLogs();
    }

    ~FoodDiary() {
        saveLogs();
    }

    // Database operations
    void loadDatabase() {
        try {
            std::ifstream file(databaseFile);
            if (!file.is_open()) {
                std::cerr << "Unable to open database file: " << databaseFile << std::endl;
                return;
            }

            json j;
            file >> j;
            file.close();

            for (const auto& item : j) {
                Food food(item);
                foods[food.name] = food;
            }
            
            std::cout << "Loaded " << foods.size() << " foods from database." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading database: " << e.what() << std::endl;
        }
    }

    // Log operations
    void loadLogs() {
        try {
            std::ifstream file(logFile);
            if (!file.is_open()) {
                std::cout << "No existing log file found. Creating a new one." << std::endl;
                return;
            }

            json j;
            file >> j;
            file.close();

            for (auto& [date, entries] : j.items()) {
                for (const auto& entry : entries) {
                    std::string foodName = entry["food"];
                    double servings = entry["servings"];
                    double calories = entry["calories"];
                    dailyLogs[date].emplace_back(foodName, servings, calories);
                }
            }
            
            std::cout << "Loaded food logs for " << dailyLogs.size() << " days." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error loading logs: " << e.what() << std::endl;
        }
    }

    void saveLogs() {
        try {
            json j;
            
            for (const auto& [date, entries] : dailyLogs) {
                json dateEntries = json::array();
                
                for (const auto& entry : entries) {
                    json entryJson;
                    entryJson["food"] = entry.foodName;
                    entryJson["servings"] = entry.servings;
                    entryJson["calories"] = entry.calories;
                    dateEntries.push_back(entryJson);
                }
                
                j[date] = dateEntries;
            }
            
            std::ofstream file(logFile);
            if (!file.is_open()) {
                std::cerr << "Unable to open log file for writing: " << logFile << std::endl;
                return;
            }
            
            file << std::setw(4) << j;
            file.close();
            
            std::cout << "Logs saved successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error saving logs: " << e.what() << std::endl;
        }
    }

    // Command to add a food entry
    class AddFoodCommand : public Command {
    private:
        FoodDiary& diary;
        std::string date;
        std::string foodName;
        double servings;
        double calories;
        
    public:
        AddFoodCommand(FoodDiary& d, const std::string& dt, const std::string& name, double servs) 
            : diary(d), date(dt), foodName(name), servings(servs) {
            // Calculate calories based on food definition
            auto it = diary.foods.find(foodName);
            if (it != diary.foods.end()) {
                calories = it->second.calories * servings;
            } else {
                calories = 0;
            }
        }
        
        void execute() override {
            diary.dailyLogs[date].emplace_back(foodName, servings, calories);
        }
        
        void undo() override {
            auto& entries = diary.dailyLogs[date];
            if (!entries.empty()) {
                // Remove the latest entry with this food name
                for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                    if (it->foodName == foodName && std::abs(it->servings - servings) < 0.001) {
                        entries.erase((it + 1).base());
                        break;
                    }
                }
            }
            
            // If the daily log is now empty, remove the date entry
            if (entries.empty()) {
                diary.dailyLogs.erase(date);
            }
        }
        
        std::string getDescription() const override {
            std::stringstream ss;
            ss << "Add " << servings << " serving(s) of " << foodName << " (" 
               << calories << " calories) on " << date;
            return ss.str();
        }
    };

    // Command to delete a food entry
    class DeleteFoodCommand : public Command {
    private:
        FoodDiary& diary;
        std::string date;
        size_t index;
        FoodEntry deletedEntry;
        
    public:
        DeleteFoodCommand(FoodDiary& d, const std::string& dt, size_t idx) 
            : diary(d), date(dt), index(idx), 
              deletedEntry("", 0, 0) {
            // Store the entry for potential undo
            auto& entries = diary.dailyLogs[date];
            if (index < entries.size()) {
                deletedEntry = entries[index];
            }
        }
        
        void execute() override {
            auto& entries = diary.dailyLogs[date];
            if (index < entries.size()) {
                entries.erase(entries.begin() + index);
                
                // If the daily log is now empty, remove the date entry
                if (entries.empty()) {
                    diary.dailyLogs.erase(date);
                }
            }
        }
        
        void undo() override {
            // Re-add the deleted entry
            diary.dailyLogs[date].push_back(deletedEntry);
        }
        
        std::string getDescription() const override {
            std::stringstream ss;
            ss << "Delete " << deletedEntry.servings << " serving(s) of " 
               << deletedEntry.foodName << " from " << date;
            return ss.str();
        }
    };

    // Date management
    void setCurrentDate(const std::string& date) {
        if (DateUtil::isValidDate(date)) {
            currentDate = date;
            std::cout << "Current date set to: " << currentDate << std::endl;
        } else {
            std::cerr << "Invalid date format. Please use YYYY-MM-DD." << std::endl;
        }
    }

    std::string getCurrentDate() const {
        return currentDate;
    }

    // Food search functions
    std::vector<std::string> searchFoodsByKeywords(const std::vector<std::string>& keywords, bool matchAll) {
        std::vector<std::string> results;
        
        for (const auto& [name, food] : foods) {
            bool matches = matchAll;
            
            for (const auto& keyword : keywords) {
                bool keywordFound = false;
                std::string lowerKeyword = toLower(keyword);
                
                // Check if keyword is in food keywords
                for (const auto& foodKeyword : food.keywords) {
                    if (toLower(foodKeyword).find(lowerKeyword) != std::string::npos) {
                        keywordFound = true;
                        break;
                    }
                }
                
                // Also check if keyword is in food name
                if (!keywordFound && toLower(food.name).find(lowerKeyword) != std::string::npos) {
                    keywordFound = true;
                }
                
                if (matchAll) {
                    matches = matches && keywordFound;
                } else {
                    matches = matches || keywordFound;
                }
                
                // Early exit for OR matching
                if (!matchAll && keywordFound) {
                    matches = true;
                    break;
                }
            }
            
            if (matches) {
                results.push_back(name);
            }
        }
        return results;
    }

    // Food management
    void listAllFoods() const {
        std::cout << "\nAll Foods in Database:\n";
        std::cout << std::setw(30) << std::left << "Name" 
                  << std::setw(15) << std::left << "Type"
                  << std::setw(15) << std::right << "Calories" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& [name, food] : foods) {
            std::cout << std::setw(30) << std::left << name 
                      << std::setw(15) << std::left << food.type
                      << std::setw(15) << std::right << food.calories << std::endl;
        }
        std::cout << std::endl;
    }

    void displayFoodDetails(const std::string& foodName) const {
        auto it = foods.find(foodName);
        if (it == foods.end()) {
            std::cout << "Food not found: " << foodName << std::endl;
            return;
        }
        
        const Food& food = it->second;
        
        std::cout << "\nFood Details: " << food.name << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Type: " << food.type << std::endl;
        std::cout << "Calories: " << food.calories << std::endl;
        
        std::cout << "Keywords: ";
        for (size_t i = 0; i < food.keywords.size(); i++) {
            std::cout << food.keywords[i];
            if (i < food.keywords.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
        
        if (food.type == "composite") {
            std::cout << "\nComponents:" << std::endl;
            for (const auto& [compName, servings] : food.components) {
                std::cout << "- " << compName << ": " << servings << " serving(s)" << std::endl;
            }
        }
        std::cout << std::endl;
    }

    // Log display
    void displayDailyLog(const std::string& date) const {
        auto it = dailyLogs.find(date);
        if (it == dailyLogs.end() || it->second.empty()) {
            std::cout << "No food entries for " << date << std::endl;
            return;
        }
        
        double totalCalories = 0.0;
        
        std::cout << "\nFood Log for " << date << ":\n";
        std::cout << std::setw(5) << std::left << "No."
                  << std::setw(30) << std::left << "Food" 
                  << std::setw(15) << std::left << "Servings"
                  << std::setw(15) << std::right << "Calories" << std::endl;
        std::cout << std::string(65, '-') << std::endl;
        
        int count = 1;
        for (const auto& entry : it->second) {
            std::cout << std::setw(5) << std::left << count++
                      << std::setw(30) << std::left << entry.foodName 
                      << std::setw(15) << std::left << entry.servings
                      << std::setw(15) << std::right << entry.calories << std::endl;
            
            totalCalories += entry.calories;
        }
        
        std::cout << std::string(65, '-') << std::endl;
        std::cout << std::setw(50) << std::left << "Total Calories:" 
                  << std::setw(15) << std::right << totalCalories << std::endl;
        std::cout << std::endl;
    }

    // Command execution with undo support
    void executeCommand(std::shared_ptr<Command> command) {
        command->execute();
        undoStack.push(command);
        std::cout << "Executed: " << command->getDescription() << std::endl;
    }

    void undo() {
        if (undoStack.empty()) {
            std::cout << "Nothing to undo." << std::endl;
            return;
        }
        
        auto command = undoStack.top();
        undoStack.pop();
        
        command->undo();
        std::cout << "Undone: " << command->getDescription() << std::endl;
    }

    // Utility functions
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        return result;
    }

    // Food entry management
    void addFood(const std::string& date, const std::string& foodName, double servings) {
        auto it = foods.find(foodName);
        if (it == foods.end()) {
            std::cerr << "Food not found: " << foodName << std::endl;
            return;
        }
        
        auto command = std::make_shared<AddFoodCommand>(*this, date, foodName, servings);
        executeCommand(command);
    }

    void deleteFood(const std::string& date, size_t index) {
        auto it = dailyLogs.find(date);
        if (it == dailyLogs.end() || index >= it->second.size()) {
            std::cerr << "Invalid food entry index." << std::endl;
            return;
        }
        
        auto command = std::make_shared<DeleteFoodCommand>(*this, date, index);
        executeCommand(command);
    }

    // User interface methods
    void addFoodToLog() {
        // First, let the user choose how to select a food
        std::cout << "\nSelect food by:\n";
        std::cout << "1. Browse all foods\n";
        std::cout << "2. Search by keywords\n";
        std::cout << "Choice: ";
        
        int choice;
        std::cin >> choice;
        std::cin.ignore();
        
        std::vector<std::string> foodOptions;
        
        if (choice == 1) {
            // List all foods for selection
            listAllFoods();
            
            // Convert map to vector for indexing
            for (const auto& [name, _] : foods) {
                foodOptions.push_back(name);
            }
        } else if (choice == 2) {
            std::cout << "Enter keywords (separated by spaces): ";
            std::string keywordInput;
            std::getline(std::cin, keywordInput);
            
            // Split input into keywords
            std::vector<std::string> keywords;
            std::stringstream ss(keywordInput);
            std::string keyword;
            while (ss >> keyword) {
                keywords.push_back(keyword);
            }
            
            if (keywords.empty()) {
                std::cout << "No keywords provided." << std::endl;
                return;
            }
            
            std::cout << "Match: 1. All keywords or 2. Any keyword? ";
            int matchChoice;
            std::cin >> matchChoice;
            std::cin.ignore();
            
            bool matchAll = (matchChoice == 1);
            foodOptions = searchFoodsByKeywords(keywords, matchAll);
            
            if (foodOptions.empty()) {
                std::cout << "No foods match the given keywords." << std::endl;
                return;
            }
            
            // Display the matching foods
            std::cout << "\nMatching Foods:\n";
            for (size_t i = 0; i < foodOptions.size(); i++) {
                std::cout << (i + 1) << ". " << foodOptions[i] << std::endl;
            }
        } else {
            std::cout << "Invalid choice." << std::endl;
            return;
        }
        
        // Let the user select a food
        if (foodOptions.empty()) {
            std::cout << "No foods available for selection." << std::endl;
            return;
        }
        
        std::cout << "\nSelect food number (1-" << foodOptions.size() << "): ";
        int foodIndex;
        std::cin >> foodIndex;
        
        if (foodIndex < 1 || foodIndex > static_cast<int>(foodOptions.size())) {
            std::cout << "Invalid food selection." << std::endl;
            return;
        }
        
        std::string selectedFood = foodOptions[foodIndex - 1];
        
        // Ask for number of servings
        std::cout << "Enter number of servings: ";
        double servings;
        std::cin >> servings;
        std::cin.ignore();
        
        if (servings <= 0) {
            std::cout << "Invalid number of servings." << std::endl;
            return;
        }
        
        // Add the food to the log
        addFood(currentDate, selectedFood, servings);
    }

    void deleteFoodFromLog() {
        displayDailyLog(currentDate);
        
        auto it = dailyLogs.find(currentDate);
        if (it == dailyLogs.end() || it->second.empty()) {
            std::cout << "No entries to delete." << std::endl;
            return;
        }
        
        std::cout << "Enter entry number to delete: ";
        int index;
        std::cin >> index;
        std::cin.ignore();
        
        if (index < 1 || index > static_cast<int>(it->second.size())) {
            std::cout << "Invalid entry number." << std::endl;
            return;
        }
        
        deleteFood(currentDate, index - 1);
    }

    void changeDate() {
        std::cout << "Enter date (YYYY-MM-DD): ";
        std::string date;
        std::cin >> date;
        std::cin.ignore();
        
        setCurrentDate(date);
    }

    void viewFoodDetails() {
        listAllFoods();
        
        std::cout << "Enter food name: ";
        std::string foodName;
        std::getline(std::cin, foodName);
        
        displayFoodDetails(foodName);
    }

    void showUndoStack() const {
        if (undoStack.empty()) {
            std::cout << "Undo stack is empty." << std::endl;
            return;
        }
        
        std::cout << "\nUndo Stack (latest first):\n";
        
        // Create a temporary stack to display in reverse order
        std::stack<std::shared_ptr<Command>> tempStack = undoStack;
        int count = 1;
        
        while (!tempStack.empty()) {
            std::cout << count++ << ". " << tempStack.top()->getDescription() << std::endl;
            tempStack.pop();
        }
        
        std::cout << std::endl;
    }

    void runMainMenu() {
        bool running = true;
        
        while (running) {
            std::cout << "\n--- Food Diary (" << currentDate << ") ---\n";
            std::cout << "1. Add Food\n";
            std::cout << "2. View Today's Log\n";
            std::cout << "3. Delete Food Entry\n";
            std::cout << "4. View Food Details\n";
            std::cout << "5. Change Current Date\n";
            std::cout << "6. Undo Last Action\n";
            std::cout << "7. View Undo Stack\n";
            std::cout << "8. Save and Exit\n";
            std::cout << "Choice: ";
            

            // cout << "\n===== Diet Assistant Menu =====\n";
            // cout << "1. Search foods\n";
            // cout << "2. View food details\n";
            // cout << "3. Add basic food\n";
            // cout << "4. Create composite food\n";
            // cout << "5. List all foods\n";
            // cout << "6. Save database\n";
            // cout << "7. View Today's Log\n";
            // cout << "8. Add Food Entry\n";
            // cout << "9. Delete Food Entry\n";
            // cout << "10. Change Current Date\n";
            // cout << "11. Undo Last Action\n";
            // cout << "12. Exit\n";

            
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            
            switch (choice) {
                case 1:
                    addFoodToLog();
                    break;
                case 2:
                    displayDailyLog(currentDate);
                    break;
                case 3:
                    deleteFoodFromLog();
                    break;
                case 4:
                    viewFoodDetails();
                    break;
                case 5:
                    changeDate();
                    break;
                case 6:
                    undo();
                    break;
                case 7:
                    showUndoStack();
                    break;
                case 8:
                    saveLogs();
                    running = false;
                    break;
                default:
                    std::cout << "Invalid choice." << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::string databaseFile = "food_database.json";
    std::string logFile = "food_log.json";
    
    // Parse command line arguments
    if (argc > 1) {
        databaseFile = argv[1];
    }
    
    if (argc > 2) {
        logFile = argv[2];
    }
    
    try {
        FoodDiary diary(databaseFile, logFile);
        diary.runMainMenu();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}