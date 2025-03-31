#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <limits>

// Include nlohmann/json for JSON handling
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Forward declarations
class Food;
class BasicFood;
class CompositeFood;

// Base Food class
class Food {
protected:
    string name;
    vector<string> keywords;
    string type;

public:
    Food(const string& name, const vector<string>& keywords, const string& type)
        : name(name), keywords(keywords), type(type) {}
    
    virtual ~Food() = default;
    
    virtual float getCalories() const = 0;
    
    const string& getName() const { return name; }
    const vector<string>& getKeywords() const { return keywords; }
    const string& getType() const { return type; }
    
    virtual json toJson() const {
        json j;
        j["name"] = name;
        j["keywords"] = keywords;
        j["type"] = type;
        j["calories"] = getCalories();
        return j;
    }
    
    virtual void display() const {
        cout << "Name: " << name << endl;
        cout << "Type: " << type << endl;
        cout << "Calories: " << getCalories() << endl;
        cout << "Keywords: ";
        for (size_t i = 0; i < keywords.size(); ++i) {
            cout << keywords[i];
            if (i < keywords.size() - 1) cout << ", ";
        }
        cout << endl;
    }
};

// Basic Food class
class BasicFood : public Food {
private:
    float calories;

public:
    BasicFood(const string& name, const vector<string>& keywords, float calories)
        : Food(name, keywords, "basic"), calories(calories) {}
    
    float getCalories() const override { return calories; }
    
    static shared_ptr<BasicFood> fromJson(const json& j) {
        string name = j["name"];
        vector<string> keywords = j["keywords"].get<vector<string>>();
        float calories = j["calories"];
        return make_shared<BasicFood>(name, keywords, calories);
    }
};

// Component for Composite Food
struct FoodComponent {
    shared_ptr<Food> food;
    float servings;
    
    FoodComponent(shared_ptr<Food> food, float servings)
        : food(food), servings(servings) {}
    
    json toJson() const {
        json j;
        j["name"] = food->getName();
        j["servings"] = servings;
        return j;
    }
};

// Composite Food class
class CompositeFood : public Food {
private:
    vector<FoodComponent> components;

public:
    CompositeFood(const string& name, const vector<string>& keywords, const vector<FoodComponent>& components)
        : Food(name, keywords, "composite"), components(components) {}
    
    float getCalories() const override {
        float totalCalories = 0.0f;
        for (const auto& component : components) {
            totalCalories += component.food->getCalories() * component.servings;
        }
        return totalCalories;
    }
    
    json toJson() const override {
        json j = Food::toJson();
        json componentsJson = json::array();
        
        for (const auto& component : components) {
            componentsJson.push_back(component.toJson());
        }
        
        j["components"] = componentsJson;
        return j;
    }
    
    void display() const override {
        Food::display();
        cout << "Components:" << endl;
        for (const auto& component : components) {
            cout << "  - " << component.food->getName() 
                 << " (" << component.servings << " serving" 
                 << (component.servings > 1 ? "s" : "") << ")" << endl;
        }
    }
    
    static shared_ptr<CompositeFood> createFromComponents(
        const string& name, 
        const vector<string>& keywords,
        const vector<FoodComponent>& components) {
        return make_shared<CompositeFood>(name, keywords, components);
    }
};

// Food Database Manager class
class FoodDatabaseManager {
private:
    unordered_map<string, shared_ptr<Food>> foods;
    string databaseFilePath;
    bool modified;
    
    void clear() {
        foods.clear();
    }

public:
    FoodDatabaseManager(const string& filePath = "food_database.json") 
        : databaseFilePath(filePath), modified(false) {}
    
        bool loadDatabase() {
            clear();
            
            ifstream file(databaseFilePath);
            if (!file.is_open()) {
                cout << "No existing database found. Starting with empty database." << endl;
                return false;
            }
            
            try {
                json j;
                file >> j;
                
                // Store the entire JSON data for each food
                unordered_map<string, json> pendingFoods;
                
                // First pass: load all basic foods and catalogue composite foods
                for (const auto& foodJson : j) {
                    string type = foodJson["type"];
                    string name = foodJson["name"];
                    
                    if (type == "basic") {
                        foods[name] = BasicFood::fromJson(foodJson);
                    } else if (type == "composite") {
                        pendingFoods[name] = foodJson;
                    }
                }
                
                // Function to recursively load a composite food and its dependencies
                function<shared_ptr<Food>(const string&)> loadCompositeFood = [&] (const string& name) -> shared_ptr<Food> {
                        // If already loaded, return it
                        if (foods.find(name) != foods.end()) {
                            return foods[name];
                        }
                        
                        // If not a pending composite food, can't load it
                        if (pendingFoods.find(name) == pendingFoods.end()) {
                            cout << "Warning: Food '" << name << "' not found." << endl;
                            return nullptr;
                        }
                        
                        // Get the food's JSON
                        json foodJson = pendingFoods[name];
                        
                        // Load all components
                        vector<FoodComponent> components;
                        for (const auto& componentJson : foodJson["components"]) {
                            string componentName = componentJson["name"];
                            float servings = componentJson["servings"];
                            
                            // Recursively load component if needed
                            shared_ptr<Food> componentFood;
                            if (foods.find(componentName) != foods.end()) {
                                componentFood = foods[componentName];
                            } else {
                                componentFood = loadCompositeFood(componentName);
                            }
                            
                            if (componentFood) {
                                components.emplace_back(componentFood, servings);
                            } else {
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
                for (const auto& [name, _] : pendingFoods) {
                    loadCompositeFood(name);
                }
                
                cout << "Database loaded: " << foods.size() << " foods." << endl;
                return true;
            } catch (const exception& e) {
                cout << "Error loading database: " << e.what() << endl;
                return false;
            }
        }
    
    bool saveDatabase() {
        try {
            json j = json::array();
            
            for (const auto& [name, food] : foods) {
                j.push_back(food->toJson());
            }
            
            ofstream file(databaseFilePath);
            if (!file.is_open()) {
                cout << "Error: Unable to open file for writing." << endl;
                return false;
            }
            
            file << j.dump(4);  // Pretty print with 4 spaces
            file.close();
            
            modified = false;
            cout << "Database saved to " << databaseFilePath << endl;
            return true;
        } catch (const exception& e) {
            cout << "Error saving database: " << e.what() << endl;
            return false;
        }
    }
    
    bool addFood(shared_ptr<Food> food) {
        string name = food->getName();
        if (foods.find(name) != foods.end()) {
            cout << "Error: A food with name '" << name << "' already exists." << endl;
            return false;
        }
        
        foods[name] = food;
        modified = true;
        return true;
    }
    
    vector<shared_ptr<Food>> searchFoods(const string& query) {
        vector<shared_ptr<Food>> results;
        string lowerQuery = query;
        transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        for (const auto& [name, food] : foods) {
            // Check if query matches name
            string lowerName = name;
            transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find(lowerQuery) != string::npos) {
                results.push_back(food);
                continue;
            }
            
            // Check if query matches any keyword
            for (const auto& keyword : food->getKeywords()) {
                string lowerKeyword = keyword;
                transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
                if (lowerKeyword.find(lowerQuery) != string::npos) {
                    results.push_back(food);
                    break;
                }
            }
        }
        
        return results;
    }
    
    shared_ptr<Food> getFood(const string& name) {
        auto it = foods.find(name);
        if (it != foods.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void listAllFoods() const {
        cout << "\n=== All Foods in Database (" << foods.size() << ") ===" << endl;
        for (const auto& [name, food] : foods) {
            cout << name << " (" << food->getType() << ") - " << food->getCalories() << " calories" << endl;
        }
        cout << "===========================" << endl;
    }
    
    bool isModified() const {
        return modified;
    }
};

// Command Line Interface class
class DietAssistantCLI {
private:
    FoodDatabaseManager dbManager;
    bool running;
    
    void displayMenu() {
        cout << "\n===== Diet Assistant Menu =====\n";
        cout << "1. Search foods\n";
        cout << "2. View food details\n";
        cout << "3. Add basic food\n";
        cout << "4. Create composite food\n";
        cout << "5. List all foods\n";
        cout << "6. Save database\n";
        cout << "7. Exit\n";
        cout << "==============================\n";
        cout << "Enter choice (1-7): ";
    }
    
    void searchFoods() {
        cout << "\nEnter search term: ";
        string query;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, query);
        
        vector<shared_ptr<Food>> results = dbManager.searchFoods(query);
        
        if (results.empty()) {
            cout << "No foods found matching '" << query << "'." << endl;
        } else {
            cout << "\n=== Search Results for '" << query << "' (" << results.size() << " found) ===" << endl;
            for (size_t i = 0; i < results.size(); ++i) {
                cout << i+1 << ". " << results[i]->getName() 
                     << " (" << results[i]->getType() << ") - " 
                     << results[i]->getCalories() << " calories" << endl;
            }
        }
    }
    
    void viewFoodDetails() {
        cout << "\nEnter food name: ";
        string name;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin, name);
        
        shared_ptr<Food> food = dbManager.getFood(name);
        if (food) {
            cout << "\n=== Food Details ===" << endl;
            food->display();
        } else {
            cout << "Food '" << name << "' not found." << endl;
        }
    }
    
    void addBasicFood() {
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
        while ((pos = keywordsStr.find(',')) != string::npos) {
            token = keywordsStr.substr(0, pos);
            token.erase(0, token.find_first_not_of(' '));
            token.erase(token.find_last_not_of(' ') + 1);
            if (!token.empty()) keywords.push_back(token);
            keywordsStr.erase(0, pos + 1);
        }
        // Add the last keyword
        keywordsStr.erase(0, keywordsStr.find_first_not_of(' '));
        keywordsStr.erase(keywordsStr.find_last_not_of(' ') + 1);
        if (!keywordsStr.empty()) keywords.push_back(keywordsStr);
        
        auto newFood = make_shared<BasicFood>(name, keywords, calories);
        if (dbManager.addFood(newFood)) {
            cout << "Basic food '" << name << "' added successfully." << endl;
        }
    }
    
    void createCompositeFood() {
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
        while ((pos = keywordsStr.find(',')) != string::npos) {
            token = keywordsStr.substr(0, pos);
            token.erase(0, token.find_first_not_of(' '));
            token.erase(token.find_last_not_of(' ') + 1);
            if (!token.empty()) keywords.push_back(token);
            keywordsStr.erase(0, pos + 1);
        }
        // Add the last keyword
        keywordsStr.erase(0, keywordsStr.find_first_not_of(' '));
        keywordsStr.erase(keywordsStr.find_last_not_of(' ') + 1);
        if (!keywordsStr.empty()) keywords.push_back(keywordsStr);
        
        bool addingComponents = true;
        while (addingComponents) {
            cout << "\nEnter component food name (or 'done' to finish): ";
            string componentName;
            getline(cin, componentName);
            
            if (componentName == "done") {
                addingComponents = false;
                continue;
            }
            
            shared_ptr<Food> componentFood = dbManager.getFood(componentName);
            if (!componentFood) {
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
        
        if (components.empty()) {
            cout << "No components added. Composite food creation cancelled." << endl;
            return;
        }
        
        auto newFood = CompositeFood::createFromComponents(name, keywords, components);
        if (dbManager.addFood(newFood)) {
            cout << "Composite food '" << name << "' created successfully." << endl;
            cout << "Total calories: " << newFood->getCalories() << endl;
        }
    }
    
    void handleExit() {
        if (dbManager.isModified()) {
            cout << "Database has unsaved changes. Save before exit? (y/n): ";
            char choice;
            cin >> choice;
            
            if (choice == 'y' || choice == 'Y') {
                dbManager.saveDatabase();
            }
        }
        
        running = false;
    }

public:
    DietAssistantCLI(const string& databasePath = "food_database.json") 
        : dbManager(databasePath), running(false) {}
    
    void start() {
        running = true;
        dbManager.loadDatabase();
        
        cout << "Welcome to Diet Assistant!" << endl;
        
        while (running) {
            displayMenu();
            
            int choice;
            cin >> choice;
            
            switch (choice) {
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
                    handleExit();
                    break;
                default:
                    cout << "Invalid choice. Please try again." << endl;
            }
        }
        
        cout << "Thank you for using Diet Assistant. Goodbye!" << endl;
    }
};

int main() {
    DietAssistantCLI dietAssistant;
    dietAssistant.start();
    return 0;
}