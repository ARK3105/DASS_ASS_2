#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <map>
#include <sstream>
#include <iomanip>
#include <stack>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <limits>

#include "json.hpp"

using namespace std;
using json = nlohmann::json;

class Food;
class BasicFood;
class CompositeFood;

// Base Food class
class Food
{
protected:
    string name;
    vector<string> keywords;
    string type;

public:
    Food(const string &name, const vector<string> &keywords, const string &type)
        : name(name), keywords(keywords), type(type) {}

    virtual ~Food() = default;

    virtual float getCalories() const = 0;

    const string &getName() const { return name; }
    const vector<string> &getKeywords() const { return keywords; }
    const string &getType() const { return type; }

    virtual json toJson() const
    {
        json j;
        j["name"] = name;
        j["keywords"] = keywords;
        j["type"] = type;
        j["calories"] = getCalories();
        return j;
    }

    virtual void display() const
    {
        cout << "Name: " << name << endl;
        cout << "Type: " << type << endl;
        cout << "Calories: " << getCalories() << endl;
        cout << "Keywords: ";
        for (size_t i = 0; i < keywords.size(); ++i)
        {
            cout << keywords[i];
            if (i < keywords.size() - 1)
                cout << ", ";
        }
        cout << endl;
    }
};

// Basic Food class
class BasicFood : public Food
{
private:
    float calories;

public:
    BasicFood(const string &name, const vector<string> &keywords, float calories)
        : Food(name, keywords, "basic"), calories(calories) {}

    float getCalories() const override { return calories; } // to override getCalories from Food.

    static shared_ptr<BasicFood> fromJson(const json &j)
    {
        string name = j["name"];
        vector<string> keywords = j["keywords"].get<vector<string>>();
        float calories = j["calories"];
        return make_shared<BasicFood>(name, keywords, calories);
    }
};

// Component for Composite Food
struct FoodComponent
{
    shared_ptr<Food> food;
    float servings;

    FoodComponent(shared_ptr<Food> food, float servings) : food(food), servings(servings) {}

    json toJson() const
    {
        json j;
        j["name"] = food->getName();
        j["servings"] = servings;
        return j;
    }
};

// Composite Food class
class CompositeFood : public Food
{
private:
    vector<FoodComponent> components;

public:
    CompositeFood(const string &name, const vector<string> &keywords, const vector<FoodComponent> &components)
        : Food(name, keywords, "composite"), components(components) {}

    float getCalories() const override
    {
        float totalCalories = 0.0f;
        for (const auto &component : components)
        {
            totalCalories += component.food->getCalories() * component.servings;
        }
        return totalCalories;
    }

    json toJson() const override
    {
        json j = Food::toJson();
        json componentsJson = json::array();

        for (const auto &component : components)
        {
            componentsJson.push_back(component.toJson());
        }

        j["components"] = componentsJson;
        return j;
    }

    void display() const override
    {
        Food::display();
        cout << "Components:" << endl;
        for (const auto &component : components)
        {
            cout << "  - " << component.food->getName()
                 << " (" << component.servings << " serving"
                 << (component.servings > 1 ? "s" : "") << ")" << endl;
        }
    }

    static shared_ptr<CompositeFood> createFromComponents(
        const string &name,
        const vector<string> &keywords,
        const vector<FoodComponent> &components)
    {
        return make_shared<CompositeFood>(name, keywords, components);
    }
};

// Food Database Manager class
class FoodDatabaseManager
{
public:
    map<string, shared_ptr<Food>> foods;

private:
    string databaseFilePath;
    bool modified;

    void clear()
    {
        foods.clear();
    }

public:
    FoodDatabaseManager(const string &filePath = "food_database.json")
        : databaseFilePath(filePath), modified(false) {}

    bool loadDatabase()
    {
        clear();

        ifstream file(databaseFilePath);
        if (!file.is_open())
        {
            cout << "No existing database found. Starting with empty database." << endl;
            return false;
        }

        try
        {
            json j;
            file >> j;

            // Store the entire JSON data for each food
            map<string, json> pendingFoods;

            // First pass: load all basic foods and catalogue composite foods
            for (const auto &foodJson : j)
            {
                string type = foodJson["type"];
                string name = foodJson["name"];

                if (type == "basic")
                {
                    foods[name] = BasicFood::fromJson(foodJson);
                }
                else if (type == "composite")
                {
                    pendingFoods[name] = foodJson;
                }
            }

            // Function to recursively load a composite food and its dependencies
            function<shared_ptr<Food>(const string &)> loadCompositeFood = [&](const string &name) -> shared_ptr<Food>
            {
                // If already loaded, return it
                if (foods.find(name) != foods.end())
                {
                    return foods[name];
                }

                // If not a pending composite food, can't load it
                if (pendingFoods.find(name) == pendingFoods.end())
                {
                    cout << "Warning: Food '" << name << "' not found." << endl;
                    return nullptr;
                }

                // Get the food's JSON
                json foodJson = pendingFoods[name];

                // Load all components
                vector<FoodComponent> components;
                for (const auto &componentJson : foodJson["components"])
                {
                    string componentName = componentJson["name"];
                    float servings = componentJson["servings"];

                    // Recursively load component if needed
                    shared_ptr<Food> componentFood;
                    if (foods.find(componentName) != foods.end())
                    {
                        componentFood = foods[componentName];
                    }
                    else
                    {
                        componentFood = loadCompositeFood(componentName);
                    }

                    if (componentFood)
                    {
                        components.emplace_back(componentFood, servings);
                    }
                    else
                    {
                        cout << "Warning: Component '" << componentName
                             << "' not found for composite food '" << name << "'" << endl;
                    }
                }

                // Create the composite food
                vector<string> keywords = foodJson["keywords"].get<vector<string>>();
                shared_ptr<Food> food = make_shared<CompositeFood>(name, keywords, components);

                // Add it to loaded foods
                foods[name] = food;

                return food;
            };

            // Second pass: load all composite foods with dependencies
            for (const auto &[name, _] : pendingFoods)
            {
                loadCompositeFood(name);
            }

            cout << "Database loaded: " << foods.size() << " foods." << endl;
            return true;
        }
        catch (const exception &e)
        {
            cout << "Error loading database: " << e.what() << endl;
            return false;
        }
    }

    bool saveDatabase()
    {
        try
        {
            json j = json::array();

            for (const auto &[name, food] : foods)
            {
                j.push_back(food->toJson());
            }

            ofstream file(databaseFilePath);
            if (!file.is_open())
            {
                cout << "Error: Unable to open file for writing." << endl;
                return false;
            }

            file << j.dump(4); // Pretty print with 4 spaces
            file.close();

            modified = false;
            cout << "Database saved to " << databaseFilePath << endl;
            return true;
        }
        catch (const exception &e)
        {
            cout << "Error saving database: " << e.what() << endl;
            return false;
        }
    }

    bool addFood(shared_ptr<Food> food)
    {
        string name = food->getName();
        if (foods.find(name) != foods.end())
        {
            cout << "Error: A food with name '" << name << "' already exists." << endl;
            return false;
        }

        foods[name] = food;
        modified = true;
        return true;
    }

    vector<shared_ptr<Food>> searchFoodsByKeywords(const vector<string> &keywords, bool matchall)
    {
        vector<shared_ptr<Food>> results;
        // if matchall is there, we need foods with all keywords, else food which atleast one keyword
        for (const auto &[name, food] : foods)
        {
            int cnt = 0;
            for (auto &keyword : keywords)
            {
                string lowerKeyword = keyword;
                transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
                for (const auto &foodKeyword : food->getKeywords())
                {
                    string lowerFoodKeyword = foodKeyword;
                    transform(lowerFoodKeyword.begin(), lowerFoodKeyword.end(), lowerFoodKeyword.begin(), ::tolower);
                    if (lowerFoodKeyword.find(lowerKeyword) != string::npos)
                    {
                        cnt++;
                        break;
                    }
                }
            }
            if (matchall && cnt == keywords.size())
            {
                results.push_back(food);
            }
            else if (!matchall && cnt > 0)
            {
                results.push_back(food);
            }
        }
        return results;
    }

    shared_ptr<Food> getFood(const string &name)
    {
        auto it = foods.find(name);
        if (it != foods.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void listAllFoods() const
    {
        cout << "\n=== All Foods in Database (" << foods.size() << ") ===" << endl;
        for (const auto &[name, food] : foods)
        {
            cout << name << " (" << food->getType() << ") - " << food->getCalories() << " calories" << endl;
        }
        cout << "===========================" << endl;
    }

    bool isModified() const
    {
        return modified;
    }
};

// Food log entry for a specific day
class FoodEntry
{
public:
    string foodName;
    double servings;
    double calories;

    FoodEntry(const string &name, double servs, double cals)
        : foodName(name), servings(servs), calories(cals) {}
};

// Date handling utility
class DateUtil
{
public:
    static string getCurrentDate()
    {
        auto now = chrono::system_clock::now();
        auto time = chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&time);

        stringstream ss;
        ss << put_time(&tm, "%Y-%m-%d");
        return ss.str();
    }

    static bool isValidDate(const string &dateStr)
    {
        if (dateStr.length() != 10)
            return false;

        // Check format: YYYY-MM-DD
        for (int i = 0; i < 10; i++)
        {
            if ((i == 4 || i == 7) && dateStr[i] != '-')
                return false;
            else if (i != 4 && i != 7 && !isdigit(dateStr[i]))
                return false;
        }

        int year = stoi(dateStr.substr(0, 4));
        int month = stoi(dateStr.substr(5, 2));
        int day = stoi(dateStr.substr(8, 2));

        if (month < 1 || month > 12)
            return false;

        int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        // Adjust for leap year
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        {
            daysInMonth[2] = 29;
        }

        return day >= 1 && day <= daysInMonth[month];
    }
};

// Command interface for undo functionality
class Command
{
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual string getDescription() const = 0;
};

// Food diary main class
class FoodDiary
{
private:
    string logFile;
    map<string, vector<FoodEntry>> dailyLogs;
    stack<shared_ptr<Command>> undoStack;
    string currentDate;
    FoodDatabaseManager &dbManager;

public:
    FoodDiary(FoodDatabaseManager &db, const string &log)
        : dbManager(db), logFile(log), currentDate(DateUtil::getCurrentDate())
    {
        loadLogs();
    }

    ~FoodDiary()
    {
        saveLogs();
    }

    // Log operations
    void loadLogs()
    {
        try
        {
            ifstream file(logFile);
            if (!file.is_open())
            {
                cout << "No existing log file found. Creating a new one." << endl;
                return;
            }

            json j;
            file >> j;
            file.close();

            for (auto &[date, entries] : j.items())
            {
                for (const auto &entry : entries)
                {
                    string foodName = entry["food"];
                    double servings = entry["servings"];
                    double calories = entry["calories"];
                    dailyLogs[date].emplace_back(foodName, servings, calories);
                }
            }

            cout << "Loaded food logs for " << dailyLogs.size() << " days." << endl;
        }
        catch (const exception &e)
        {
            cerr << "Error loading logs: " << e.what() << endl;
        }
    }

    void saveLogs()
    {
        try
        {
            json j;

            for (const auto &[date, entries] : dailyLogs)
            {
                json dateEntries = json::array();

                for (const auto &entry : entries)
                {
                    json entryJson;
                    entryJson["food"] = entry.foodName;
                    entryJson["servings"] = entry.servings;
                    entryJson["calories"] = entry.calories;
                    dateEntries.push_back(entryJson);
                }

                j[date] = dateEntries;
            }

            ofstream file(logFile);
            if (!file.is_open())
            {
                cerr << "Unable to open log file for writing: " << logFile << endl;
                return;
            }

            file << setw(4) << j;
            file.close();

            cout << "Logs saved successfully." << endl;
        }
        catch (const exception &e)
        {
            cerr << "Error saving logs: " << e.what() << endl;
        }
    }

    // Command to add a food entry
    class AddFoodCommand : public Command
    {
    private:
        FoodDiary &diary;
        string date;
        string foodName;
        double servings;
        double calories;

    public:
        AddFoodCommand(FoodDiary &d, const string &dt, const string &name, double servs)
            : diary(d), date(dt), foodName(name), servings(servs)
        {
            // Calculate calories based on food definition
            // auto it = diary.foods.find(foodName);
            auto it = diary.dbManager.getFood(foodName);
            if (it != nullptr)
            {
                calories = it->getCalories() * servings;
            }
            else
            {
                // cerr << "Food not found: " << foodName << endl;
                calories = 0;
            }
        }

        void execute() override
        {
            diary.dailyLogs[date].emplace_back(foodName, servings, calories);
        }

        void undo() override
        {
            auto &entries = diary.dailyLogs[date];
            if (!entries.empty())
            {
                // Remove the latest entry with this food name
                for (auto it = entries.rbegin(); it != entries.rend(); ++it)
                {
                    if (it->foodName == foodName && abs(it->servings - servings) < 0.001)
                    {
                        entries.erase((it + 1).base());
                        break;
                    }
                }
            }

            // If the daily log is now empty, remove the date entry
            if (entries.empty())
            {
                diary.dailyLogs.erase(date);
            }
        }

        string getDescription() const override
        {
            stringstream ss;
            ss << "Add " << servings << " serving(s) of " << foodName << " ("
               << calories << " calories) on " << date;
            return ss.str();
        }
    };

    // Command to delete a food entry
    class DeleteFoodCommand : public Command
    {
    private:
        FoodDiary &diary;
        string date;
        size_t index;
        FoodEntry deletedEntry;

    public:
        DeleteFoodCommand(FoodDiary &d, const string &dt, size_t idx)
            : diary(d), date(dt), index(idx),
              deletedEntry("", 0, 0)
        {
            // Store the entry for potential undo
            auto &entries = diary.dailyLogs[date];
            if (index < entries.size())
            {
                deletedEntry = entries[index];
            }
        }

        void execute() override
        {
            auto &entries = diary.dailyLogs[date];
            if (index < entries.size())
            {
                entries.erase(entries.begin() + index);
                // If the daily log is now empty, remove the date entry
                if (entries.empty())
                {
                    diary.dailyLogs.erase(date);
                }
            }
        }

        void undo() override
        {
            // Re-add the deleted entry
            diary.dailyLogs[date].push_back(deletedEntry);
        }

        string getDescription() const override
        {
            stringstream ss;
            ss << "Delete " << deletedEntry.servings << " serving(s) of "
               << deletedEntry.foodName << " from " << date;
            return ss.str();
        }
    };

    // Date management
    void setCurrentDate(const string &date)
    {
        if (DateUtil::isValidDate(date))
        {
            currentDate = date;
            cout << "Current date set to: " << currentDate << endl;
        }
        else
        {
            cerr << "Invalid date format. Please use YYYY-MM-DD." << endl;
        }
    }

    string getCurrentDate() const
    {
        return currentDate;
    }

    // Log display
    void displayDailyLog(const string &date) const
    {
        auto it = dailyLogs.find(date);
        if (it == dailyLogs.end() || it->second.empty())
        {
            cout << "No food entries for " << date << endl;
            return;
        }

        double totalCalories = 0.0;

        cout << "\nFood Log for " << date << ":\n";
        cout << setw(5) << left << "No."
             << setw(30) << left << "Food"
             << setw(15) << left << "Servings"
             << setw(15) << right << "Calories" << endl;
        cout << string(65, '-') << endl;

        int count = 1;
        for (const auto &entry : it->second)
        {
            cout << setw(5) << left << count++
                 << setw(30) << left << entry.foodName
                 << setw(15) << left << entry.servings
                 << setw(15) << right << entry.calories << endl;

            totalCalories += entry.calories;
        }

        cout << string(65, '-') << endl;
        cout << setw(50) << left << "Total Calories:"
             << setw(15) << right << totalCalories << endl;
        cout << endl;
    }

    // Command execution with undo support
    void executeCommand(shared_ptr<Command> command)
    {
        command->execute();
        undoStack.push(command);
        cout << "Executed: " << command->getDescription() << endl;
    }

    void undo()
    {
        if (undoStack.empty())
        {
            cout << "Nothing to undo." << endl;
            return;
        }

        auto command = undoStack.top();
        undoStack.pop();

        command->undo();
        cout << "Undone: " << command->getDescription() << endl;
    }

    // Food entry management
    void addFood(const string &date, const string &foodName, double servings)
    {
        auto it = dbManager.getFood(foodName);
        if (!it)
        {
            cerr << "Food not found: " << foodName << endl;
            return;
        }

        auto command = make_shared<AddFoodCommand>(*this, date, foodName, servings);
        executeCommand(command);
    }

    void deleteFood(const string &date, size_t index)
    {
        auto it = dailyLogs.find(date);
        if (it == dailyLogs.end() || index >= it->second.size())
        {
            cerr << "Invalid food entry index." << endl;
            return;
        }

        auto command = make_shared<DeleteFoodCommand>(*this, date, index);
        executeCommand(command);
    }

    // User interface methods
    void addFoodToLog()
    {
        // First, let the user choose how to select a food
        cout << "\nSelect food by:\n";
        cout << "1. Browse all foods\n";
        cout << "2. Search by keywords\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;
        cin.ignore();

        vector<string> foodOptions;

        if (choice == 1)
        {
            // List all foods for selection
            dbManager.listAllFoods();

            // Convert map to vector for indexing
            for (const auto &[name, food] : dbManager.foods)
            {
                foodOptions.push_back(name);
            }
        }
        else if (choice == 2)
        {
            cout << "Enter keywords (separated by spaces): ";
            string keywordInput;
            getline(cin, keywordInput);

            // Split input into keywords
            vector<string> keywords;
            stringstream ss(keywordInput);
            string keyword;
            while (ss >> keyword)
            {
                keywords.push_back(keyword);
            }

            if (keywords.empty())
            {
                cout << "No keywords provided." << endl;
                return;
            }

            cout << "Match: 1. All keywords or 2. Any keyword? ";
            int matchChoice;
            cin >> matchChoice;
            cin.ignore();

            bool matchAll = (matchChoice == 1);
            auto vec = dbManager.searchFoodsByKeywords(keywords, matchAll);
            for (const auto &food : vec)
            {
                foodOptions.push_back(food->getName());
            }

            if (foodOptions.empty())
            {
                cout << "No foods match the given keywords." << endl;
                return;
            }

            // Display the matching foods
            cout << "\nMatching Foods:\n";
            for (size_t i = 0; i < foodOptions.size(); i++)
            {
                cout << (i + 1) << ". " << foodOptions[i] << endl;
            }
        }
        else
        {
            cout << "Invalid choice." << endl;
            return;
        }

        // Let the user select a food
        if (foodOptions.empty())
        {
            cout << "No foods available for selection." << endl;
            return;
        }

        cout << "\nSelect food number (1-" << foodOptions.size() << "): ";
        int foodIndex;
        cin >> foodIndex;

        if (foodIndex < 1 || foodIndex > static_cast<int>(foodOptions.size()))
        {
            cout << "Invalid food selection." << endl;
            return;
        }

        string selectedFood = foodOptions[foodIndex - 1];

        // Ask for number of servings
        cout << "Enter number of servings: ";
        double servings;
        cin >> servings;
        cin.ignore();

        if (servings <= 0)
        {
            cout << "Invalid number of servings." << endl;
            return;
        }

        // Add the food to the log
        addFood(currentDate, selectedFood, servings);
    }

    void deleteFoodFromLog()
    {
        displayDailyLog(currentDate);

        auto it = dailyLogs.find(currentDate);
        if (it == dailyLogs.end() || it->second.empty())
        {
            cout << "No entries to delete." << endl;
            return;
        }

        cout << "Enter entry number to delete: ";
        int index;
        cin >> index;
        cin.ignore();

        if (index < 1 || index > static_cast<int>(it->second.size()))
        {
            cout << "Invalid entry number." << endl;
            return;
        }

        deleteFood(currentDate, index - 1);
    }

    void changeDate()
    {
        cout << "Enter date (YYYY-MM-DD): ";
        string date;
        cin >> date;
        cin.ignore();

        setCurrentDate(date);
    }

    void showUndoStack() const
    {
        if (undoStack.empty())
        {
            cout << "Undo stack is empty." << endl;
            return;
        }

        cout << "\nUndo Stack (latest first):\n";

        // Create a temporary stack to display in reverse order
        stack<shared_ptr<Command>> tempStack = undoStack;
        int count = 1;

        while (!tempStack.empty())
        {
            cout << count++ << ". " << tempStack.top()->getDescription() << endl;
            tempStack.pop();
        }

        cout << endl;
    }
};

// Command Line Interface class
class DietAssistantCLI
{
private:
    FoodDatabaseManager dbManager;
    FoodDiary foodDiary;
    bool running;

    void displayMenu()
    {
        cout << "\n===== Diet Assistant Menu =====\n";
        cout << "1. Search foods\n";
        cout << "2. View food details\n";
        cout << "3. Add basic food\n";
        cout << "4. Create composite food\n";
        cout << "5. List all foods\n";
        cout << "6. Save database\n";
        cout << "7. View Today's Log\n";
        cout << "8. Add Food Entry\n";
        cout << "9. Delete Food Entry\n";
        cout << "10. Change Current Date\n";
        cout << "11. Undo Last Action\n";
        cout << "12. Exit\n";
        cout << "==============================\n";
        cout << "Enter choice (1-12): ";
    }

    void searchFoods()
    {
        cout << "1. Do you want to search by keywords? (yes/no): ";
        string choice;
        cin >> choice;
        if (choice == "yes") {
            cout << "Enter keywords (separated by spaces): ";
            string keywordInput;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            getline(cin, keywordInput);

            // Split input into keywords
            vector<string> keywords;
            stringstream ss(keywordInput);
            string keyword;
            while (ss >> keyword)
            {
                keywords.push_back(keyword);
            }

            if (keywords.empty())
            {
                cout << "No keywords provided." << endl;
                return;
            }

            cout << "Match: 1. All keywords or 2. Any keyword? ";
            int matchChoice;
            cin >> matchChoice;

            bool matchAll = (matchChoice == 1);
            auto vec = dbManager.searchFoodsByKeywords(keywords, matchAll);
            for (const auto &food : vec)
            {
                cout << food->getName() << " (" << food->getType() << ") - "
                     << food->getCalories() << " calories" << endl;
            }
        }
        else {
            cout << "Enter food name: ";
            string name;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            getline(cin, name);

            shared_ptr<Food> food = dbManager.getFood(name);
            if (food)
            {
                cout << "\n=== Food Details ===" << endl;
                food->display();
            }
            else
            {
                cout << "Food '" << name << "' not found." << endl;
            }
        }
    }

    void viewFoodDetails()
    {
        cout << "\nEnter food name: ";
        string name;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, name);

        shared_ptr<Food> food = dbManager.getFood(name);
        if (food)
        {
            cout << "\n=== Food Details ===" << endl;
            food->display();
        }
        else
        {
            cout << "Food '" << name << "' not found." << endl;
        }
    }

    void addBasicFood()
    {
        string name;
        vector<string> keywords;
        float calories;

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << "\n=== Add Basic Food ===" << endl;

        cout << "Enter food name: ";
        getline(cin, name);

        cout << "Enter calories per serving: ";
        cin >> calories;

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << "Enter keywords (comma-separated): ";
        string keywordsStr;
        getline(cin, keywordsStr);

        // Parse comma-separated keywords
        size_t pos = 0;
        string token;
        while ((pos = keywordsStr.find(',')) != string::npos)
        {
            token = keywordsStr.substr(0, pos);
            token.erase(0, token.find_first_not_of(' '));
            token.erase(token.find_last_not_of(' ') + 1);
            if (!token.empty())
                keywords.push_back(token);
            keywordsStr.erase(0, pos + 1);
        }
        // Add the last keyword
        keywordsStr.erase(0, keywordsStr.find_first_not_of(' '));
        keywordsStr.erase(keywordsStr.find_last_not_of(' ') + 1);
        if (!keywordsStr.empty())
            keywords.push_back(keywordsStr);

        auto newFood = make_shared<BasicFood>(name, keywords, calories);
        if (dbManager.addFood(newFood))
        {
            cout << "Basic food '" << name << "' added successfully." << endl;
        }
    }

    void createCompositeFood()
    {
        string name;
        vector<string> keywords;
        vector<FoodComponent> components;

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << "\n=== Create Composite Food ===" << endl;

        cout << "Enter composite food name: ";
        getline(cin, name);

        cout << "Enter keywords (comma-separated): ";
        string keywordsStr;
        getline(cin, keywordsStr);

        // Parse comma-separated keywords
        size_t pos = 0;
        string token;
        while ((pos = keywordsStr.find(',')) != string::npos)
        {
            token = keywordsStr.substr(0, pos);
            token.erase(0, token.find_first_not_of(' '));
            token.erase(token.find_last_not_of(' ') + 1);
            if (!token.empty())
                keywords.push_back(token);
            keywordsStr.erase(0, pos + 1);
        }
        // Add the last keyword
        keywordsStr.erase(0, keywordsStr.find_first_not_of(' '));
        keywordsStr.erase(keywordsStr.find_last_not_of(' ') + 1);
        if (!keywordsStr.empty())
            keywords.push_back(keywordsStr);

        bool addingComponents = true;
        while (addingComponents)
        {
            cout << "\nEnter component food name (or 'done' to finish): ";
            string componentName;
            getline(cin, componentName);

            if (componentName == "done")
            {
                addingComponents = false;
                continue;
            }

            shared_ptr<Food> componentFood = dbManager.getFood(componentName);
            if (!componentFood)
            {
                cout << "Food '" << componentName << "' not found." << endl;
                continue;
            }

            float servings;
            cout << "Enter number of servings: ";
            cin >> servings;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            components.emplace_back(componentFood, servings);
            cout << "Added " << servings << " serving" << (servings > 1 ? "s" : "")
                 << " of '" << componentName << "'" << endl;
        }

        if (components.empty())
        {
            cout << "No components added. Composite food creation cancelled." << endl;
            return;
        }

        auto newFood = CompositeFood::createFromComponents(name, keywords, components);
        if (dbManager.addFood(newFood))
        {
            cout << "Composite food '" << name << "' created successfully." << endl;
            cout << "Total calories: " << newFood->getCalories() << endl;
        }
    }

    void handleExit()
    {
        if (dbManager.isModified())
        {
            cout << "Database has unsaved changes. Save before exit? (y/n): ";
            char choice;
            cin >> choice;

            if (choice == 'y' || choice == 'Y')
            {
                dbManager.saveDatabase();
            }
        }

        running = false;
    }

public:
DietAssistantCLI(const string &databasePath = "food_database.json", const string &logPath = "food_log.json")
    : dbManager(databasePath), foodDiary(dbManager, logPath), running(false)
{}


    void start()
    {
        running = true;
        dbManager.loadDatabase();

        cout << "Welcome to Diet Assistant!" << endl;

        while (running)
        {
            displayMenu();

            int choice;
            cin >> choice;

            switch (choice)
            {
            case 1:
                searchFoods();
                break;
            case 2:
                viewFoodDetails();
                break;
            case 3:
                addBasicFood();
                break;
            case 4:
                createCompositeFood();
                break;
            case 5:
                dbManager.listAllFoods();
                break;
            case 6:
                dbManager.saveDatabase();
                break;
            case 7:
                foodDiary.displayDailyLog(foodDiary.getCurrentDate());
                break;
            case 8:
                foodDiary.addFoodToLog();
                break;
            case 9:
                foodDiary.deleteFoodFromLog();
                break;
            case 10:
                foodDiary.changeDate();
                break;
            case 11:
            // should undo
                foodDiary.undo();
                break;
            case 12:
                handleExit();
                break;
            default:
                cout << "Invalid choice. Please try again." << endl;
            }
        }

        cout << "Thank you for using Diet Assistant. Goodbye!" << endl;
    }
};

int main()
{
    DietAssistantCLI dietAssistant;
    dietAssistant.start();
    return 0;
}