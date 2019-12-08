#ifndef DATABASE_H
#define DATABASE_H

#include <tuple>
#include <unordered_map>
#include "utils.hpp"
#include "expression.hpp"
#include "ropchain.hpp"
#include "arch.hpp"

using std::tuple;
using std::make_tuple;
using std::unordered_map;

typedef int gadget_t; 

#define NO_GADGET -1

#define DB_MAX_REGS 64

// Types of gadgets supported in database
enum class GadgetType{
    NOP,
    // Register to register
    MOV_CST,    // reg <- cst
    MOV_REG,    // reg <- reg
    AMOV_CST,   // reg <- reg OP cst
    AMOV_REG,   // reg <- reg OP reg
    // Read from memory
    LOAD,       // reg <- mem(reg + offset)
    ALOAD,      // reg OP<- mem(reg)
    // Store to memory
    STORE,      // mem(reg + offset) <- reg
    ASTORE,     // mem(reg + offset) OP<- reg
    // jump
    JMP,     // PC <- reg
};

// Generic database for different kinds of gadgets
int find_insert_index(vector<gadget_t>& gadget_list, int gadget_num, vector<Gadget*> all);

template<class K> 
class BaseDB{
public:
    unordered_map<K, vector<gadget_t>> db;
    
    // Template methods
    void add(K key, Gadget* gadget, vector<Gadget*>& all){
        vector<gadget_t>::iterator it;
        int index;
        if( db.count(key) > 0 ){
            index = find_insert_index(db[key], gadget->id, all);
            db[key].insert(db[key].begin()+index, gadget->id);
        }else{
            db[key] = vector<gadget_t>{gadget->id};
        }
    }

    vector<gadget_t>& get(K key){
        typename unordered_map<K, vector<gadget_t>>::iterator it;
        if( (it = db.find(key)) == db.end()){
            db[key] = vector<gadget_t>{};
            return db[key];
        }else{
            return it->second;
        }
    }
};

// Big gadget database
class GadgetDB{
public:
    // Global gadgets lisst (gadgets are owned)
    vector<Gadget*> all;
    // Databases for all different gadgets types
    BaseDB<tuple<reg_t, cst_t>> mov_cst;
    BaseDB<tuple<reg_t, reg_t>> mov_reg;
    BaseDB<tuple<reg_t, reg_t, Op, cst_t>> amov_cst;
    BaseDB<tuple<reg_t, reg_t, Op, reg_t>> amov_reg;
    BaseDB<tuple<reg_t, reg_t, addr_t>> load;
    BaseDB<tuple<reg_t, Op, reg_t, addr_t>> aload;
    BaseDB<tuple<reg_t, addr_t, reg_t>> store;
    BaseDB<tuple<reg_t, addr_t, Op, reg_t>> astore;
    BaseDB<tuple<reg_t>> jmp;

    // Add and classify a gadget in the database
    gadget_t add(Gadget* gadget);
    Gadget* get(gadget_t gadget_num);

    // Destructor
    ~GadgetDB();
};


#endif