#include "Symbolic.hpp"

using namespace std; 

// Arguments for REIL-type of operations 
SymArg::SymArg(ArgType t, int i, int s): _type(t), _id(i), _size(s){
    _low = 0; 
    _high = _size-1; 
}
SymArg::SymArg(ArgType t, int i, int s, int l, int h): _type(t), _id(i), _size(s), _low(l), _high(h){}
ArgType SymArg::type(){return _type;}
int SymArg::id(){return _id;}
int SymArg::size(){return _size;}
int SymArg::low(){return _low;}
int SymArg::high(){return _high;}
cst_t SymArg::value(){
    if( _type != ARG_CST )
        throw "Error, value() should not be called for this class";
    else
        return _value; 
}

ArgEmpty::ArgEmpty(): SymArg(ARG_EMPTY, -1, -1){}

ArgCst::ArgCst(cst_t v, int s): SymArg(ARG_CST, -1, s){
    _value = v; 
}

ArgReg::ArgReg(int n, int s): SymArg(ARG_REG, n, s){}
ArgReg::ArgReg(int n, int s, int l, int h): SymArg(ARG_REG, n, s, l, h){}

ArgTmp::ArgTmp( int n, int s): SymArg(ARG_TMP, n, s){}
ArgTmp::ArgTmp( int n, int s, int l, int h): SymArg(ARG_TMP, n, s, l, h){}

// IR Instruction
IRInstruction::IRInstruction(IROperation o, SymArg a1, SymArg a2, SymArg d): _op(o), _src1(a1), _src2(a2), _dst(d){}
IROperation IRInstruction::op(){return _op;}
SymArg* IRInstruction::src1(){return &_src1;}
SymArg* IRInstruction::src2(){return &_src2;}
SymArg* IRInstruction::dst(){return &_dst;}

// Some useful functions 
bool is_calculation_instr(IRInstruction& instr){
    return  instr.op() == IR_ADD || 
            instr.op() == IR_AND ||
            instr.op() == IR_BSH || 
            instr.op() == IR_DIV || 
            instr.op() == IR_MOD || 
            instr.op() == IR_MUL || 
            instr.op() == IR_OR ||
            instr.op() == IR_SUB ||
            instr.op() == IR_XOR ;    
}

// IR Block of instructions 
IRBlock::IRBlock(){
    int i;
    for( i = 0; i < NB_REGS_MAX; i++ ){
        _reg_modified[i] = false;
        _reg_table[i] = nullptr; 
    }
}

bool IRBlock::add_instr(IRInstruction ins){
    if( _instr.size() == NB_INSTR_MAX )
        return false;
    else
        _instr.push_back(ins);
        return true;
}

vector<SPair>* IRBlock::arg_to_spairs(SymArg& arg ){
    vector<SPair>* res, *res2;
    ExprObjectPtr expr; 
    vector<SPair>::iterator tmp;
     
    if( arg.type() == ARG_CST ){
        expr = NewExprCst(arg.value(), arg.size());
        res = new vector<SPair>();
        res->push_back(SPair(expr, NewCondTrue()));
    }else if( arg.type() == ARG_TMP){
        res = new vector<SPair>(*_tmp_table[arg.id()]);
    }else if( arg.type() == ARG_REG){
        if( _reg_modified[arg.id()])
            res = new vector<SPair>(*_reg_table[arg.id()]);
        else{
            // We create a new value for it
            expr = make_shared<ExprObject>(make_shared<ExprReg>(arg.id(), arg.size()));
            _reg_table[arg.id()] = new vector<SPair>();
            _reg_table[arg.id()]->push_back(SPair(expr, NewCondTrue()));
            res = _reg_table[arg.id()]; 
        }
    }else{
        throw "SymArg type not supported in arg_to_expr()";
    }
    // Translate if low and high specified 
    if( arg.low() != 0 || arg.high() != arg.size()-1){
        res2 = new vector<SPair>(); 
        for( tmp = res->begin(); tmp != res->end(); tmp++){
            res2->push_back(SPair(Extract((*tmp).expr(), arg.high(), arg.low()),  (*tmp).cond()));
        }
        delete res; 
        res = nullptr; 
        return res2; 
    }else
        return res; 
}

inline ExprObjectPtr IRBlock::full_reg_assignment(ExprObjectPtr expr, ExprObjectPtr prev, SymArg& reg){
    if( reg.low() == 0 && reg.high() == reg.size()-1)
        return expr; 
    else if( reg.low() == 0 ){
        return Concat(Extract(prev, reg.size()-1, reg.high()+1), expr);
    }else if( reg.high() == reg.size()-1){
        return Concat(expr, Extract(prev, reg.low()-1, 0));
    }else{
        return Concat( Extract(prev, reg.size()-1, reg.high()+1), Concat(expr, Extract(prev, reg.low()-1, 0)));
    }    
}

// Pre-condition: all expressions in spairs have the same size
vector<SPair>* IRBlock::full_reg_assignment(vector<SPair>* spairs, SymArg& reg){
    vector<SPair>::iterator p, p2; 
    vector<SPair>* res, *prev;
    if( spairs->empty() || ( reg.low() == 0 && reg.high() == reg.size()-1))
        return new vector<SPair>(*spairs); 
    res = new vector<SPair>();
    prev = arg_to_spairs(reg);
    for(p = spairs->begin(); p != spairs->end(); p++ ){
        for(p2 = prev->begin(); p2 != spairs->end(); p2++ )
            res->push_back(SPair(full_reg_assignment((*p).expr(), (*p2).expr(), reg), (*p).cond()));
    }
    return res; 
}


vector<SPair>* IRBlock::execute_calculation(IROperation op, vector<SPair>* src1, vector<SPair>*src2){
    vector<SPair>* res = new vector<SPair>(); 
    vector<SPair>::iterator arg1, arg2; 
    for( arg1 = src1->begin(); arg1 != src1->end(); arg1++){
        for( arg2 = src2->begin(); arg2 != src2->end(); arg2++){
            // Compute their combinaison 
            switch(op){
                case IR_ADD:
                    res->push_back( SPair((*arg1).expr()+(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ; 
                    break;
                case IR_AND:
                    res->push_back( SPair((*arg1).expr()&(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_BSH:
                    res->push_back( SPair(Bsh((*arg1).expr(),(*arg2).expr()), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_DIV:
                    res->push_back( SPair((*arg1).expr()/(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_MOD:
                    res->push_back( SPair((*arg1).expr()%(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_MUL:
                    res->push_back( SPair((*arg1).expr()*(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_OR:
                    res->push_back( SPair((*arg1).expr()|(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_SUB:
                    res->push_back( SPair((*arg1).expr()-(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                case IR_XOR:
                    res->push_back( SPair((*arg1).expr()^(*arg2).expr(), (*arg1).cond() && (*arg2).cond())) ;
                    break;
                default:
                    delete res; 
                    res = nullptr; 
                    throw "Unknown type of calculation in IR in combine_args()";
            }
        }
    }
    return res; 
}

void IRBlock::execute_stm(vector<SPair>* src1, vector<SPair>* dst, int& mem_write_cnt, int size){
    int i;
    vector<SPair>::iterator value, addr, it;
    vector<SPair> *tmp, *prev, *tmp2;
    ExprObjectPtr prev_addr; 
    CondObjectPtr no_overwrite_cond; 
    // Get the possible values for the write address 
    for( addr = dst->begin(); addr != dst->end(); addr++){
        if( mem_write_cnt >= NB_MEM_MAX )
            throw "Too many memory writes!";
        tmp = new vector<SPair>();        
        // Get values for this write 
        for( value = src1->begin();  value != src1->end(); value++)
            if( (*value).expr()->expr_ptr()->size() != size ) 
                tmp->push_back(SPair(Extract((*value).expr(), size-1, 0), (*value).cond() && (*addr).cond()));
            else
                tmp->push_back(SPair((*value).expr(), (*value).cond() && (*addr).cond()));
        _mem_table[mem_write_cnt++] = make_pair((*addr).expr(), tmp);
        // Update all the previous ones to add the non-overwritten condition 
        for( i = mem_write_cnt-2; i >= 0; i--){
            // Get the i-th access SPairs
            std::tie(prev_addr, prev) = _mem_table[i];
            no_overwrite_cond = (((*addr).expr()+NewExprCst((cst_t)(size-1), (*addr).expr()->expr_ptr()->size())) < prev_addr)
                            || 
                        ((prev_addr+NewExprCst((cst_t)(prev_addr->expr_ptr()->size()-1), prev_addr->expr_ptr()->size())) < (*addr).expr());
            // For each previous possible value, and the non-overwritten condition 
            for( value = prev->begin(); value != prev->end(); value++){
                (*value).set_cond( (*value).cond() && no_overwrite_cond );
            }
            // Add those possible values if overwritten
            for( it = tmp->begin(); it != tmp->end(); it++){
                prev->push_back( SPair( (*it).expr(), (*it).cond() && ((*addr).expr() == prev_addr)));
            }
        }
    }
}

vector<SPair>* IRBlock::execute_ldm(SPair& spair, int size, int mem_write_cnt){
    int i, write_size;
    ExprObjectPtr write_addr; 
    vector<SPair>* pairs, *res; 
    ExprObjectPtr addr = spair.expr();
    CondObjectPtr no_overwrite_cond = NewCondTrue();
    CondObjectPtr equal_cond, nequal_cond;
    vector<SPair>::iterator it; 
    
    
    res = new vector<SPair>(); 
    // For each memory access
    for( i = mem_write_cnt-1; i >= 0; i--){
        tie(write_addr, pairs) = _mem_table[i]; 
        // Get the condition for equal addresses
        equal_cond = (write_addr == addr);
        // Add the possible value(s)
        for(it = pairs->begin(); it != pairs->end(); it++){
            // FIXEME ? Here we approximate if we read more than what we wrote
            if( size >= (*it).expr()->expr_ptr()->size())
                res->push_back(SPair((*it).expr(),  (*it).cond() && equal_cond  && spair.cond() ));
            else
                res->push_back(SPair(Extract((*it).expr(), size-1, 0 ),  (*it).cond() && equal_cond  && spair.cond()));
        }
        write_size = (*pairs)[0].expr()->expr_ptr()->size(); 
        // Update the condition if different addresses
        nequal_cond =   ((write_addr+NewExprCst((cst_t)(write_size-1), write_addr->expr_ptr()->size())) < addr)
                        || 
                        ((addr+NewExprCst((cst_t)(size-1), addr->expr_ptr()->size())) < write_addr);
        no_overwrite_cond = no_overwrite_cond && nequal_cond; 
    } 
    // If all writes don't match the read, the value is the initial memory 
    res->push_back(SPair(NewExprMem( addr, size), no_overwrite_cond));
    return res; 
}

Semantics* IRBlock::compute_semantics(){
    vector<class IRInstruction>::iterator it; 
    Semantics* res;
    vector<SPair>* src1, *src2, *comb, *dst, *mem, *tmp;
    vector<SPair>::iterator pit;
    ExprObjectPtr addr; 
    int mem_write_cnt = 0, i; 
    for( it = _instr.begin(); it != _instr.end(); ++it){
        if( is_calculation_instr((*it))){
            // Get src1 and src2
            src1 = this->arg_to_spairs(*(it->src1())); 
            src2 = this->arg_to_spairs(*(it->src2()));
            // Compute their combinaison 
            comb = this->execute_calculation(it->op(), src1, src2);
            
            if( it->dst()->type() == ARG_REG ){
                _reg_table[it->dst()->id()] = this->full_reg_assignment(comb, *(it->dst()));
                _reg_modified[it->dst()->id()] = true; 
            }else if( it->dst()->type() == ARG_TMP )
                _tmp_table[it->dst()->id()] = comb; 
            else
                throw "Invalid arg type for dst in IR calculation instruction"; 
        }else if( it->op() == IR_STR ){
            src1 = this->arg_to_spairs(*(it->src1())); 
            if( it->dst()->type() == ARG_REG ){
                _reg_table[it->dst()->id()] = this->full_reg_assignment(src1, *(it->dst()));
                _reg_modified[it->dst()->id()] = true;
            }else if( it->dst()->type() == ARG_TMP )
                _tmp_table[it->dst()->id()] = src1; 
            else
                throw "Invalid arg type for dst in IR_STR instruction"; 
        }else if( it->op() == IR_STM ){
            src1 = this->arg_to_spairs(*(it->src1()));
            dst = this->arg_to_spairs(*(it->dst()));
            execute_stm(src1, dst, mem_write_cnt, it->src1()->size());
        }else if( it->op() == IR_LDM){
            src1 = this->arg_to_spairs(*(it->src1()));
            mem = new vector<SPair>();
            // For all possible read address values, get the semantics  
            for(pit = src1->begin(); pit != src1->end(); pit++){
                tmp = execute_ldm(*pit, it->dst()->size(), mem_write_cnt);
                mem->insert(mem->end(), std::make_move_iterator(tmp->begin()), std::make_move_iterator(tmp->end()));
                delete tmp;
                tmp = nullptr;  
            }
            if( it->dst()->type() == ARG_REG ){
                _reg_table[it->dst()->id()] = mem;
                _reg_modified[it->dst()->id()] = true; 
            }else if( it->dst()->type() == ARG_TMP )
                _tmp_table[it->dst()->id()] = mem; 
            else
                throw "Invalid arg type for dst in IR_LDM instruction"; 
        }
    }
    // TODO: fill the semantic object and return it
    res = new Semantics();
    // Register semantics 
    for( i = 0; i < NB_REGS_MAX; i++ ){
        if( _reg_modified[i] ){
            res->add_reg( i, _reg_table[i]);
        }else if( _reg_table[i] != nullptr ){
             delete _reg_table[i]; 
             _reg_table[i] = nullptr; 
        }
    }
    // Memory semantics 
    for( i = 0; i < mem_write_cnt; i++){
        std:tie(addr, tmp) = _mem_table[i];
        res->add_mem(addr, tmp);
    }
    return res; 
}
