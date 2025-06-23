#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// Estrutura para passar comandos atraves de funcoes recursivas.
struct history {
    int flags;
};

static struct token *token_next();
static struct token *token_peek_next();
struct node* node_create(struct node* node_ptr);
void node_push(struct node* node);
struct node* node_pop(); 
struct node* node_peek(); 
struct node* node_peek_expressionable_or_null();
void make_exp_node(struct node* left, struct node* right, const char* op);
void node_set_vector(struct vector* node_vec, struct vector* node_tree_vec); 

void parse_expressionable(struct history *history);
int parse_expressionable_single(struct history *history);
static bool parser_left_op_has_priority(const char *op_left, const char *op_right);
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERADOR_GROUPS];
int parse(struct compile_process *process);
void parse_identifier(struct history* history);
static bool keyword_is_datatype(const char* val);
static bool is_keyword_variable_modifier(const char* val);
static void expected_sym(char c);
bool token_is_primitive_keyword(struct token* token);
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secundary_token);
int parser_datatype_expected_for_type_string(const char* str);
struct token*parser_build_random_type_name();
int parser_get_pointer_depth();
bool parser_datatype_is_secondary_allowed_for_type(const char* type);
void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token);
void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out);
void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parse_datatype_type(struct datatype* dtype);
void parse_datatype_modifiers(struct datatype* dtype);
void parse_datatype(struct datatype* dtype);
void parse_variable_function_or_struct_union(struct history* history);
void parse_keyword(struct history *history);

void parse_struct(struct history* history);
static bool token_next_is_operator(const char* op);
void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
void parse_expressionable_root(struct history* history);
void parse_if(struct history* history);
void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history);

static struct compile_process *current_process;
static struct token *parser_last_token;
struct history *history_begin(int flags)
{
    struct history *history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}
struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

static void parser_ignore_nl_or_comment(struct token *token)
{
    while (token && discart_token(token))
    {
        // Pula o token que deve ser descartado no vetor.
        vector_peek(current_process->token_vec);
        // Pega o proximo token para ver se ele tambem sera descartado.
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos; // Atualiza a posicao do arquivo de compilacao.
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}

void parse_single_token_to_node()
{
    struct token *token = token_next();
    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        node = node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = token->llnum});
        break;
    case TOKEN_TYPE_IDENTIFIER:
        node = node_create(&(struct node){.type = NODE_TYPE_IDENTIFIER, .sval = token->sval});
        break;
    case TOKEN_TYPE_STRING:
        node = node_create(&(struct node){.type = NODE_TYPE_STRING, .sval = token->sval});
        break;
    default:
        compiler_error(current_process, "Esse token nao pode ser convertido para node!\n");
        break;
    }

    // Verifica se o nó foi criado com sucesso
    if (node)
    {
        node_push(node);
    }
    else
    {
        printf("ERRO: Falha ao criar nó para token: %d\n", token->type);
    }
}

void parse_expressionable_for_op(struct history *history, const char *op)
{
    if (!op) return;
    parse_expressionable(history);
}

void parser_node_shift_children_left(struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right == NODE_TYPE_EXPRESSION);

    const char *right_op = node->exp.right->exp.op;
    struct node *new_exp_left_node = node->exp.left;
    struct node *new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    // EX: 50*E(20+50) -> E(50*20)+50
    struct node *new_left_operand = node_pop();
    struct node *new_right_operand = node->exp.right->exp.right;
    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}

void parser_reorder_expression(struct node **node_out)
{
    struct node *node = *node_out;

    // Only reorder expression nodes
    if (!node || node->type != NODE_TYPE_EXPRESSION)
        return;

    // Recurse into sub-expressions first
    if (node->exp.left && node->exp.left->type == NODE_TYPE_EXPRESSION)
        parser_reorder_expression(&node->exp.left);

    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
        parser_reorder_expression(&node->exp.right);

    // Now check if the right expression has higher precedence
    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
    {
        const char *current_op = node->exp.op;
        const char *right_op = node->exp.right->exp.op;

        if (parser_left_op_has_priority(current_op, right_op))
        {
            // Get nodes A, B, C
            struct node *A = node->exp.left;
            struct node *B = node->exp.right->exp.left;
            struct node *C = node->exp.right->exp.right;

            // Save the operator from the right expression
            const char *new_op = right_op;

            // Create (A op B) subtree
            make_exp_node(A, B, current_op);
            struct node *new_left = node_pop();

            // Update current node: (A op B) new_op C
            node->exp.left = new_left;
            node->exp.right = C;
            node->exp.op = new_op;

            // Check if we need to further reorder the new left subtree
            parser_reorder_expression(&node->exp.left);
        }
    }
}

static int parser_get_precedence_for_operator(const char *op, struct expressionable_op_precedence_group **group_out)
{
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERADOR_GROUPS; i++)
    {
        for (int j = 0; op_precedence[i].operators[j]; j++)
        {
            const char *_op = op_precedence[i].operators[j];
            if (S_EQ(op, _op))
            {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

static bool parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    struct expressionable_op_precedence_group *group_left = NULL;
    struct expressionable_op_precedence_group *group_right = NULL;

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);

    // Compara os precedentes
    if (precedence_left != precedence_right)
    {
        return precedence_left < precedence_right;
    }

    // Igual -> Confere associatividade
    if (group_left && group_left->associativity == ASSOCIATIVITY_LEFT_TO_RIGTH)
    {
        return false;
    }

    // Associatividade direita -> esquerda prioridade
    if (group_left && group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        return true;
    }

    return false;
}

void parse_exp_normal(struct history *history)
{
    struct token *op_token = token_peek_next();
    if (!op_token) return;
    const char *op = op_token->sval;
    if (!op) return;

    struct node *node_left = node_peek_expressionable_or_null();
    if (!node_left) return;

    // Retira da lista de tokens o token de operador. Ex: "123+456", retira o token "+".
    token_next();

    // Retira da lista de nodes, o ultimo node inserido.
    node_pop();

    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Nesse momento, temos o node da esquerda e o operador. Essa funcao ira criar o node da direita.
    parse_expressionable_for_op(history_down(history, history->flags), op);
    struct node *node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Cria o node de expressao, passando o node da esquerda, node da direita e operador.
    make_exp_node(node_left, node_right, op);
    struct node *exp_node = node_pop();

    // TODO: Inserir AQUI codigo para reordenador a expressao.
    parser_reorder_expression(&exp_node); // LAB4

    node_push(exp_node);
}

int parse_exp(struct history *history)
{
    parse_exp_normal(history);
    return 0;
}

void parse_keyword_for_global() {
    parse_keyword(history_begin(0));
    // TODO: Essa funcao ainda nao cria o node corretamente.
    struct node* node = node_pop();
}

void parse_identifier(struct history* history) {
    struct token* token = token_peek_next();
    if (!token || token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Esperado identificador\n");
        return;
    }
    parse_single_token_to_node();
}

static bool keyword_is_datatype(const char* val) { // LAB5
    return S_EQ(val, "void") ||
           S_EQ(val, "char") ||
           S_EQ(val, "int") ||
           S_EQ(val, "short") ||
           S_EQ(val, "float") ||
           S_EQ(val, "double") ||
           S_EQ(val, "long") ||
           S_EQ(val, "struct") ||
           S_EQ(val, "union");
}

static bool is_keyword_variable_modifier(const char* val) { // LAB5
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const") ||
           S_EQ(val, "extern") ||
           S_EQ(val, "__ignore_typecheck__");
}

static void expected_sym(char c){
    struct token *next_token = token_next();

    if (!next_token || next_token->type != TOKEN_TYPE_SYMBOL || next_token->cval != c)
        compiler_error(current_process, "ERRO: Esperado o simbolo '%c'!", c);
}

bool token_is_primitive_keyword(struct token* token) { // LAB5
    if (!token) return false;
    if (token->type != TOKEN_TYPE_KEYWORD) return false;
    if (S_EQ(token->sval, "void")) {
        return true;
    } else if (S_EQ(token->sval, "char")) {
        return true;
    } else if (S_EQ(token->sval, "short")) {
        return true;
    } else if (S_EQ(token->sval, "int")) {
        return true;
    } else if (S_EQ(token->sval, "long")) {
        return true;
    } else if (S_EQ(token->sval, "float")) {
        return true;
    } else if (S_EQ(token->sval, "double")) {
        return true;
    }
    return false;
}

// Essa funcao trata o caso de 2 datatypes seguidos. Ex: long int A.
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secundary_token) { // LAB5
    *datatype_token = token_next();
    struct token* next_token = token_peek_next();
    if (token_is_primitive_keyword(next_token)) {
        *datatype_secundary_token = next_token;
        token_next();
    }
}

int parser_datatype_expected_for_type_string(const char* str) { // LAB5
    int type = DATATYPE_EXPECT_PRIMITIVE;
    if (S_EQ(str, "union")) {
        type = DATATYPE_EXPECT_UNION;
    } else if (S_EQ(str, "struct")) {
        type = DATATYPE_EXPECT_STRUCT;
    }
    return type;
}

struct token* parser_build_random_type_name() { // LAB5
    static int num1 = 0, num2 = 0, num3 = 0;
    static char letter1 = 'a', letter2 = 'a';

    // Atualiza os índices para a próxima chamada
    num3++;
    if (num3 > 99) {
        num3 = 0;
        letter2++;
        if (letter2 > 'z') {
            letter2 = 'a';
            num2++;
            if (num2 > 99) {
                num2 = 0;
                letter1++;
                if (letter1 > 'z') {
                    letter1 = 'a';
                    num1++;
                    if (num1 > 99) {
                        num1 = 0;
                    }
                }
            }
        }
    }

    // Gera o nome customtypename_<num3>_<letter2>_<num2>_<letter1>_<num1>
    char tmp_name[64];
    snprintf(tmp_name, sizeof(tmp_name), "customtypename_%d_%c_%d_%c_%d", num1, letter1, num2, letter2, num3);
    
    char *sval = malloc(sizeof(tmp_name));
    strncpy(sval, tmp_name, sizeof(tmp_name));

    struct token* token = calloc(1, sizeof(struct token));
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = sval;

    return token;
}

int parser_get_pointer_depth() {// LAB5
    int depth = 0;
    if(!token_peek_next())
    {
        compiler_error(current_process, "ERRO: Esperado ponteiro, mas nao ha mais tokens.");
    }
    while (token_next_is_operator("*")) {
        depth++;
        token_next();
    }
    return depth;
}

bool parser_datatype_is_secondary_allowed_for_type(const char* type) {
    return S_EQ(type, "long") || S_EQ(type, "short") || S_EQ(type, "double") || S_EQ(type, "float");
}

int parse_expressionable_single(struct history *history) {
    struct token *token = token_peek_next();
    if (!token) return -1;
    
    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;
    
    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            parse_single_token_to_node();
            res = 0;
            break;
        case TOKEN_TYPE_IDENTIFIER:
            parse_identifier(history);
            res = 0;
            break;
        case TOKEN_TYPE_OPERATOR:
            parse_exp(history);
            res = 0;
            break;
        case TOKEN_TYPE_STRING:
            parse_single_token_to_node();
            res = 0;
            break;
        case TOKEN_TYPE_SYMBOL: // Tratamento para parênteses em expressões
            if (token->cval == '(') {
                token_next(); // Consome '('
                parse_expressionable_root(history_down(history, history->flags));
                struct node* inner_exp = node_pop();
                struct node* paren_node = node_create(&(struct node){.type = NODE_TYPE_EXPRESSION_PARENTHESES, .exp.left = inner_exp, .pos = token->pos});
                node_push(paren_node);
                expected_sym(')'); // Espera ')'
                res = 0;
            } else {
                // Para outros símbolos que não iniciam uma expressão ou são operadores unários
                // Se for um ';' ou '}', significa o fim de um statement ou bloco, então não deve ser processado aqui.
                // Isso requer um controle mais robusto no chamador (parse_expressionable)
                // ou um tratamento explícito para símbolos unários.
                res = -1; // Indica que este token não é parte de uma "single expressionable"
            }
            break;
        case TOKEN_TYPE_KEYWORD:
            // Para keywords dentro de expressões, apenas avança
            token_next();
            res = 0;
            break;
        default:
            // Para outros tipos, apenas avança
            token_next();
            res = 0;
            break;
    }
    return res;
}

void parse_expressionable(struct history *history)
{
    int max_iterations = 100; // Limite para evitar loop infinito
    int iteration_count = 0;
    
    while (iteration_count < max_iterations)
    {
        struct token* next_token = token_peek_next();
        if (!next_token) break;
        // Se o próximo token não for algo que possa iniciar ou continuar uma expressão, saia.
        // Operadores e parênteses podem continuar uma expressão.
        // Números, identificadores, strings podem iniciar uma expressão.
        bool is_expression_continuation =
            (next_token->type == TOKEN_TYPE_OPERATOR) ||
            (token_is_symbol(next_token, '(')) ||
            (token_is_symbol(next_token, '[')) || // Para acesso a array
            (token_is_symbol(next_token, '.')); // Para acesso a membro de struct
        bool is_expression_start =
            (next_token->type == TOKEN_TYPE_NUMBER) ||
            (next_token->type == TOKEN_TYPE_IDENTIFIER) ||
            (next_token->type == TOKEN_TYPE_STRING) ||
            (token_is_symbol(next_token, '(')); // Parênteses podem iniciar uma sub-expressão
        // if (!is_expression_start && !is_expression_continuation && vector_count(node_stack) > 0) {
        if (!is_expression_start && !is_expression_continuation && vector_count(current_process->node_vec) > 0) {
            // Se já temos um nó na pilha (iniciamos uma expressão) e o próximo token
            // não pode continuar a expressão, então a expressão terminou.
            break;
        //} else if (!is_expression_start && vector_count(node_stack) == 0) {
        } else if (!is_expression_start && vector_count(current_process->node_vec) == 0) {
        // Se não temos nada na pilha e o token não pode iniciar uma expressão,
            // então não há expressão a ser parseada aqui.
            break;
        }
        if (parse_expressionable_single(history) != 0) {
            // parse_expressionable_single retorna -1 se o token não é processável como "single"
            break;
        }
        iteration_count++;
    }
    
    if (iteration_count >= max_iterations) {
        printf("Aviso: Número máximo de iterações atingido em parse_expressionable\n");
    }
}

int parse_next()
{
    struct token *token = token_peek_next();
    if (!token)
        return -1;

    int res = 0;
    switch (token->type)
    {
    case TOKEN_TYPE_KEYWORD:
        if (S_EQ(token->sval, "struct")) {
            token_next(); // Consome "struct"
            parse_struct(history_begin(0));
            res = 0;
        } else if (S_EQ(token->sval, "if")) {
            token_next(); // Consome "if"
            parse_if(history_begin(0));
            res = 0;
        } else if (S_EQ(token->sval, "return")) {
            token_next(); // Consome "return"
            // Processa o valor de retorno
            parse_expressionable_root(history_begin(0));
            struct node* return_value = node_pop();
            struct node return_node_struct = { // Usar um nome diferente para evitar conflito com 'return_node' local
                .type = NODE_TYPE_STATEMENT_RETURN,
                .pos = token->pos
            };
            struct node* created_return = node_create(&return_node_struct);

            if (created_return) {
                // Adiciona o valor de retorno como filho esquerdo do nó de retorno
                created_return->exp.left = return_value;
                node_push(created_return);
                printf("Statement de retorno processado\n");
            }

            // Verifica ponto e vírgula
            expected_sym(';');
            res = 0;
        } else if (keyword_is_datatype(token->sval) || is_keyword_variable_modifier(token->sval)) {
            parse_variable_function_or_struct_union(history_begin(0));
            res = 0;
        } else {
            // Para outros keywords, apenas avança
            printf("Aviso: Keyword nao tratada explicitamente em parse_next: %s\n", token->sval);
            token_next();
            res = 0;
        }
        break;
    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_IDENTIFIER:
    case TOKEN_TYPE_STRING:
    case TOKEN_TYPE_OPERATOR:
        parse_expressionable(history_begin(0));
        // Após uma expressão, normalmente esperamos um ponto e vírgula
        expected_sym(';');
        res = 0;
        break;
    case TOKEN_TYPE_SYMBOL:
        if (token->cval == '{' || token->cval == '}') {
            // Ignora chaves de blocos que não são tratadas por uma função específica (e.g., função main)
            // Em um parser completo, isso estaria dentro de uma função de parsing de bloco.
            token_next();
            res = 0;
        } else if (token->cval == ';') {
            // Ponto e vírgula solto (empty statement)
            token_next();
            res = 0;
        } else if (token->cval == '(') { // Pode ser uma expressão entre parênteses
            parse_expressionable(history_begin(0));
            expected_sym(';');
             res = 0;
        } else {
            compiler_error(current_process, "Simbolo nao esperado ou nao tratado em parse_next: '%c'\n", token->cval);
            res = -1;
        }
        break;
    default:
        // Avança o token se não conseguir processar
        printf("Aviso: Token nao tratavel em parse_next, avançando: Tipo %d, Valor '%s'\n", token->type, token->sval ? token->sval : "");
        token_next();
        res = 0;
        break;
    }
    return res;
}

// Para imprimir árvore e seus nós
void print_node_type(struct node *node)
{
    if (!node)
    {
        printf("NULL");
        return;
    }

    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        printf("NUMERO  (%llu)", node->llnum);
        break;
    case NODE_TYPE_IDENTIFIER:
        printf("IDENTIFICADOR  (%s)", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_STRING:
        printf("STRING (\"%s\")", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_EXPRESSION:
        printf("EXPRESSAO (op: %s)", node->exp.op ? node->exp.op : "null");
        break;
    case NODE_TYPE_UNARY:
        printf("UNARIO  (op: %s)", node->exp.op ? node->exp.op : "null");
        break;
    case NODE_TYPE_EXPRESSION_PARENTHESES:
        printf("EXPRESSAO_PARENTESES");
        break;
    case NODE_TYPE_VARIABLE:
        {
            printf("VARIABLE (name: %s, type: %s", 
                   node->var.name ? node->var.name : "null",
                   node->var.type.type_str ? node->var.type.type_str : "null");
            
            // Mostrar informações de array se aplicável
            if (node->var.type.flags & DATATYPE_FLAG_IS_ARRAY) {
                printf(", ARRAY[%zu", node->var.type.size);
                
                // Mostrar dimensões adicionais se houver
                struct datatype* current = node->var.type.datatype_secondary;
                while (current && current->size > 0) {
                    printf("][%zu", current->size);
                    current = current->datatype_secondary;
                }
                printf("]");
            }
            
            // Mostrar valor inicial se houver
            if (node->var.val) {
                printf(", inicializado");
            }
            
            printf(")\n");
        }
        break;
    case NODE_TYPE_VARIABLE_LIST:
        printf("LISTA_VARIAVEL  (count: %d)", 
               node->var_list.list ? vector_count(node->var_list.list) : 0);
        break;
    case NODE_TYPE_STRUCT:
        printf("STRUCT (nome: %s)", node->sval ? node->sval : "null");
        break;
    case NODE_TYPE_STATEMENT_IF:
        printf("DECLARACAO_IF");
        break;
    case NODE_TYPE_STATEMENT_RETURN:
        printf("DECLARACAO_RETURN");
        break;
    default:
        printf("DESCONHECIDO (tipo %d)", node->type);
        break;
    }
}

void print_node_tree(struct node *node, int indent, bool is_last)
{
    if (!node) {
        for (int i = 0; i < indent; ++i) {
            printf("%s", is_last ? " " : "| ");
        }
        printf("%sNULL node\n", is_last ? "`-- " : "+-- ");
        return;
    }

    for (int i = 0; i < indent; ++i) {
        printf("%s", is_last ? "  " : "| ");
    }
    printf("%s", is_last ? "`-- " : "+-- ");

    print_node_type(node);
    printf("\n");

    // Imprimir filhos baseado no tipo do nó
    switch (node->type)
    {
        case NODE_TYPE_EXPRESSION:
        case NODE_TYPE_UNARY:
            print_node_tree(node->exp.left, indent + 1, !node->exp.right);
            print_node_tree(node->exp.right, indent + 1, true);
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES: // EXPRESSAO_PARENTESES tem apenas um filho (a expressão interna)
            print_node_tree(node->exp.left, indent + 1, true);
            break;
        case NODE_TYPE_VARIABLE:
            if (node->var.val)
            {
                print_node_tree(node->var.val, indent + 1, true);
            }
            break;
            
        case NODE_TYPE_VARIABLE_LIST:
            if (node->var_list.list)
            {
                for (int i = 0; i < vector_count(node->var_list.list); i++)
                {
                    struct node** var_ptr = vector_at(node->var_list.list, i);
                    if (var_ptr && *var_ptr)
                    {
                        print_node_tree(*var_ptr, indent + 1, i == vector_count(node->var_list.list) - 1);
                    }
                }
            }
            break;
            
        case NODE_TYPE_STRUCT:
            // Aqui você pode adicionar a impressão dos membros da struct se necessário
            break;
        case NODE_TYPE_STATEMENT_IF:
            // Condição do if está em exp.left
            // Aqui você pode adicionar a impressão da condição e do corpo do if se necessário
            printf("   %sCondicao:\n", is_last ? "   " : "|  ");
            print_node_tree(node->exp.left, indent + 2, false); // Condição
            
            // Corpo do if está em exp.right (que é um NODE_TYPE_VARIABLE_LIST acting as a statement list)
            printf("   %sCorpo:\n", is_last ? "   " : "|  ");
            if (node->exp.right && node->exp.right->type == NODE_TYPE_VARIABLE_LIST) {
                // Itera sobre os statements no corpo do if
                struct vector* body_statements = node->exp.right->var_list.list;
                if (body_statements && vector_count(body_statements) > 0) {
                    for (int i = 0; i < vector_count(body_statements); i++) {
                        struct node** stmt_ptr = vector_at(body_statements, i);
                        if (stmt_ptr && *stmt_ptr) {
                            print_node_tree(*stmt_ptr, indent + 3, i == vector_count(body_statements) - 1);
                        }
                    }
                } else {
                     for (int i = 0; i < indent + 3; ++i) {
                         printf("%s", is_last ? "   " : "|  ");
                     }
                     printf("`-- Corpo vazio\n");
                }
            } else {
                 for (int i = 0; i < indent + 2; ++i) {
                     printf("%s", is_last ? "   " : "|  ");
                 }
                 printf("`-- Corpo vazio ou malformado\n");
            }
            break;
        case NODE_TYPE_STATEMENT_RETURN:
            if (node->exp.left) { // O valor de retorno é o filho esquerdo
                print_node_tree(node->exp.left, indent + 1, true);
            }
            break;
    }
}

int parse(struct compile_process *process)
{
    // Retorna caso tenha algum erro
    if (!process || !process->node_tree_vec || !process->token_vec || !process->node_vec)
    {
        return PARSE_GENERAL_ERROR;
    }
    current_process = process;
    parser_last_token = NULL;
    node_set_vector(process->node_vec, process->node_tree_vec);
    vector_set_peek_pointer(process->token_vec, 0);

    // Imprime arvore de nodes
    printf("\n\nArvore de nodes:\n");
    
    // Contador de segurança para evitar loop infinito
    int max_iterations = 1000; // Número máximo de iterações
    int iteration_count = 0;
    
    // Processa cada token até o final
    while (iteration_count < max_iterations)
    {
        struct token* token = token_peek_next();
        if (!token) break;

        // Processa o próximo token
        if (parse_next() != 0) break;

        // Obtém o nó criado
        struct node* node = node_peek();
        if (!node)
        {
            printf("Warning: node_peek() returned NULL (iteracao %d)\n", iteration_count);
            // Incrementa o contador mesmo quando node_peek retorna NULL
            iteration_count++;
            continue;
        }

        // Adiciona o nó à árvore
        vector_push(process->node_tree_vec, &node);

        // Imprime a árvore
        printf("--- No raiz #%d ---\n", vector_count(process->node_tree_vec) - 1);
        print_node_tree(node, 0, true);
        printf("\n");
        
        iteration_count++;
    }

    if (iteration_count >= max_iterations) {
        printf("Aviso: Número máximo de iterações atingido. Possível loop infinito detectado.\n");
    }

    return PARSE_ALL_OK;
}

void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token) {
    if (!datatype_secondary_token) return;
    struct datatype* secondary_data_type = calloc(1, sizeof(struct datatype));
    if (!secondary_data_type) {
        compiler_error(current_process, "Falha ao alocar memoria para datatype secundario\n");
        return;
    }
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, NULL, secondary_data_type);
    datatype->size += secondary_data_type->size;
    datatype->datatype_secondary = secondary_data_type;
    datatype->flags |= DATATYPE_FLAG_IS_SECONDARY;
}

void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out) {
    if (!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval) && datatype_secondary_token) {
        compiler_error(current_process, "Voce utilizou um datatype secundario invalido!\n");
    }
    if (S_EQ(datatype_token->sval, "void")) {
        datatype_out->type = DATATYPE_VOID;
        datatype_out->size = 0;
    } else if (S_EQ(datatype_token->sval, "char")) {
        datatype_out->type = DATATYPE_CHAR;
        datatype_out->size = 1; // 1 BYTE
    } else if (S_EQ(datatype_token->sval, "short")) {
        datatype_out->type = DATATYPE_SHORT;
        datatype_out->size = 2; // 2 BYTES
    } else if (S_EQ(datatype_token->sval, "int")) {
        datatype_out->type = DATATYPE_INTEGER;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "long")) {
        datatype_out->type = DATATYPE_LONG;
        datatype_out->size = 8; // 8 BYTES
    } else if (S_EQ(datatype_token->sval, "float")) {
        datatype_out->type = DATATYPE_FLOAT;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "double")) {
        datatype_out->type = DATATYPE_DOUBLE;
        datatype_out->size = 8; // 8 BYTES
    } else {
        compiler_error(current_process, "BUG: Datatype invalido!\n");
    }
    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    if (!(expected_type == DATATYPE_EXPECT_PRIMITIVE) && datatype_secondary_token) {
        compiler_error(current_process, "Voce utilizou um datatype secundario invalido!\n");
    }
    switch (expected_type) {
        case DATATYPE_EXPECT_PRIMITIVE:
            parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
            break;
        case DATATYPE_EXPECT_STRUCT:
        case DATATYPE_EXPECT_UNION:
            // Placeholder: Em uma implementação completa, você buscaria a definição da struct/union
            // para obter seu tamanho e membros. Por enquanto, define um tamanho padrão ou 0.
            if (expected_type == DATATYPE_EXPECT_STRUCT) {
                datatype_out->type = DATATYPE_STRUCT;
                datatype_out->size = 0; // Tamanho será preenchido após parsing da struct
            } else {
                datatype_out->type = DATATYPE_UNION;
                datatype_out->size = 0; // Tamanho será preenchido após parsing da union
            }
        default:
            compiler_error(current_process, "BUG: Erro desconhecido na inicializacao do datatypeo!\n");
    }
    // Para ponteiros, o tamanho é o tamanho de um endereço de memória (assumindo 8 bytes para 64-bit)
    if (pointer_depth > 0) {
        datatype_out->size = 8;
    }
    datatype_out->pointer_depth = pointer_depth;
}

void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;
}

void parse_datatype_type(struct datatype* dtype) { // LAB5
    struct token* datatype_token = NULL;
    struct token* datatype_secundary_token = NULL;
    parser_get_datatype_tokens(&datatype_token, &datatype_secundary_token);
    
    if (!datatype_token) {
        compiler_error(current_process, "Esperado tipo após modificadores\n");
        return;
    }

    int expected_type = parser_datatype_expected_for_type_string(datatype_token->sval);
    datatype_secundary_token = datatype_token; // Será o token do nome do tipo (e.g., "int", "MyStruct")

    if (S_EQ(datatype_token->sval, "union") || S_EQ(datatype_token->sval, "struct")) {
        struct token* next_token = token_peek_next();
        if (!next_token) {
            compiler_error(current_process, "Esperado identificador após struct/union\n");
            return;
        }
        if (next_token && next_token->type == TOKEN_TYPE_IDENTIFIER) {
            // Caso da struct/union com nome. Ex: `struct MyStruct { ... };`
            datatype_token = token_next(); // Consome o nome da struct/union
        } else { // Caso da struct sem nome -> gerar nome aleatorio.
            // Caso da struct/union sem nome (anônima). Ex: `struct { int x; } my_var;`

            datatype_token = parser_build_random_type_name();
            dtype->flags |= DATATYPE_FLAG_IS_STRUCT_UNION_NO_NAME;
        }
    }
    // Descobre a quantidade de ponteiros.
    int pointer_depth = parser_get_pointer_depth();
    parser_datatype_init(datatype_token, datatype_secundary_token, dtype, pointer_depth, expected_type);
}

void parse_datatype_modifiers(struct datatype* dtype) { // LAB5
    struct token* token = token_peek_next();
    if (!token) return;

    while (token && token->type == TOKEN_TYPE_KEYWORD) {
        if (!is_keyword_variable_modifier(token->sval)) break;
        if (S_EQ(token->sval, "signed")) {
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "unsigned")) {
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "static")) {
            dtype->flags |= DATATYPE_FLAG_IS_STATIC;
        } else if (S_EQ(token->sval, "const")) {
            dtype->flags |= DATATYPE_FLAG_IS_CONST;
        } else if (S_EQ(token->sval, "extern")) {
            dtype->flags |= DATATYPE_FLAG_IS_EXTERN;
        } else if (S_EQ(token->sval, "__ignore_typecheck__")) {
            dtype->flags |= DATATYPE_FLAG_IS_IGNORE_TYPE_CHECKING;
        }
        token_next();
        token = token_peek_next();
    }
}

void parse_datatype(struct datatype* dtype) { // LAB5
    memset(dtype, 0, sizeof(struct datatype)); 
    // Flag padrao.
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED; // Assumir signed por padrão
    parse_datatype_modifiers(dtype);
    parse_datatype_type(dtype);
    parse_datatype_modifiers(dtype);
}

void parse_variable_function_or_struct_union(struct history* history) {
    struct datatype dtype;
    parse_datatype(&dtype);  // Analisa o tipo de dado e modificadores
    struct token* name_token = token_next(); // Pega o nome da variável/função/struct
    if (!name_token || name_token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Variavel ou funcao declarada sem nome!\n");
        return;
    }
    parse_variable(&dtype, name_token, history);

    // Após o nome, precisamos verificar se é uma variável, função ou definição de struct/union
    // TODO: Implementar outros tipos de keywords (if, while, etc)
    struct token* next_token = token_peek_next();
    if (next_token && token_is_symbol(next_token, '(')) {
        // Possível função
        compiler_error(current_process, "Declaracao de funcao ainda nao implementada!\n");
        // token_next(); // Consome '('
        // parse_function_arguments();
        // ...
    } else if (next_token && token_is_symbol(next_token, '{') && (dtype.type == DATATYPE_STRUCT || dtype.type == DATATYPE_UNION)) {
        // Isso seria o caso de uma definição de struct/union declarada em linha (e.g. `struct MyStruct { int x; } var;`)
        // O caso `struct { int x; }` (anônima) já é tratado em `parse_datatype_type` gerando um nome aleatório.
        // O caso `struct MyStruct { ... };` (definição) é tratado em `parse_next` pela keyword "struct".
        // Esta parte pode precisar de mais lógica dependendo de como você quer modelar structs/unions.
        compiler_error(current_process, "Definicao de struct/union em linha ainda nao implementada!\n");
    }
    else {
        // É uma declaração de variável
        parse_variable(&dtype, name_token, history);
    }
}

void parse_keyword(struct history *history) { // LAB 5
    struct token* token = token_peek_next();
    if (!token) return;

    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval)) {
        // Cria um novo node para a declaração
        struct node* node = node_create(&(struct node){
            .type = NODE_TYPE_VARIABLE,
            .pos = token->pos
        });
        
        if (!node) {
            compiler_error(current_process, "Falha ao criar node para declaração\n");
            return;
        }

        // Parse o datatype
        struct datatype dtype;
        parse_datatype(&dtype);
        
        // Armazena o datatype no node
        node->any = malloc(sizeof(struct datatype));
        if (!node->any) {
            compiler_error(current_process, "Falha ao alocar memória para datatype\n");
            return;
        }
        memcpy(node->any, &dtype, sizeof(struct datatype));

        // Adiciona o node à pilha
        node_push(node);
        return;
    }

    // TODO: Implementar outros tipos de keywords (if, while, etc)
    compiler_error(current_process, "Keyword não implementada: %s\n", token->sval);
}

static bool token_next_is_operator(const char* op) {
    struct token* token = token_peek_next();
    return token_is_operator(token, op);
}

void parse_expressionable_root(struct history* history) {
    parse_expressionable(history);
    struct node* result_node = node_pop();
    if (result_node) {
        node_push(result_node);
    } else {
        printf("Aviso: parse_expressionable_root nao retornou um no valido. Isso pode indicar uma expressao vazia ou erro.\n");
    }
}

void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history) {
    // Criar lista de variáveis usando a estrutura varlist
    struct node var_list_node = {
        .type = NODE_TYPE_VARIABLE_LIST,
        .var_list.list = vector_create(sizeof(struct node*)),
        .pos = name_token->pos // Posição do primeiro identificador
    };
    
    if (!var_list_node.var_list.list) {
        compiler_error(current_process, "Falha ao criar lista de variáveis\n");
        return;
    }
    
    // Adicionar a primeira variável
    struct node* value_node = NULL;
    int array_dims[8] = {0}; // Suporta até 8 dimensões
    int array_dim_count = 0;
    
    // Processa colchetes para arrays
    while (token_next_is_operator("[")) {
        token_next(); // Consome o '['
        parse_expressionable_root(history_down(history, history->flags)); // Analisa a expressão dentro dos colchetes
        struct node* dim_node = node_pop(); // Pega o nó da dimensão

		
        if (dim_node && dim_node->type == NODE_TYPE_NUMBER) {
            array_dims[array_dim_count++] = dim_node->llnum;
        } else {
            // Caso a dimensão não seja um número (e.g., uma variável ou expressão complexa)
            // Você precisaria armazenar o nó da expressão para avaliação posterior.
            compiler_error(current_process, "Esperado numero inteiro ou expressao constante para tamanho do array\n");
            break;
        }

        // array_dims[array_dim_count++] = size_token->inum;
        array_dims[array_dim_count++] = dim_node->inum;
        expected_sym(']');  // Espera o ']'
    }
    
    if (token_next_is_operator("=")) {
        token_next();
        parse_expressionable_root(history);
        value_node = node_pop();
    }
    
    // Criar node para a primeira variável
    make_variable_node(dtype, name_token, value_node);
    struct node* var_node = node_pop();
    if (!var_node) {
        compiler_error(current_process, "Falha ao criar node para variavel\n");
        vector_free(var_list_node.var_list.list);
        return;
    }
    
    // Armazenar as dimensões do array no node
    if (array_dim_count > 0) {
        var_node->var.type.flags |= DATATYPE_FLAG_IS_ARRAY;
        // Para arrays multidimensionais, o `pointer_depth` pode indicar o número de dimensões.
        // O `size` do `datatype` principal é o tamanho da primeira dimensão.
        // `datatype_secondary` seria uma cadeia de datatypes para as dimensões subsequentes.
        var_node->var.type.pointer_depth = array_dim_count;
        struct datatype* current_dim_dt = &var_node->var.type; // Começa com o datatype da própria variável
        for (int i = 0; i < array_dim_count; i++) {
            // Usar o campo size para a primeira dimensão, e o campo datatype_secondary para as demais
            if (i == 0) {
                current_dim_dt->size = array_dims[0];
            } else {
                current_dim_dt->datatype_secondary = calloc(1, sizeof(struct datatype));
                if (!current_dim_dt->datatype_secondary) {
                    compiler_error(current_process, "Falha ao alocar memoria para datatype secundario de array\n");
                    break;
                }
                current_dim_dt = current_dim_dt->datatype_secondary;
                current_dim_dt->size = array_dims[i];
                current_dim_dt->type = DATATYPE_INTEGER; // Tipo padrão para dimensão de array
                current_dim_dt->flags |= DATATYPE_FLAG_IS_SIGNED;
            }
        }
    }
    vector_push(var_list_node.var_list.list, &var_node);

    // Verificar se há mais variáveis separadas pela vírgula
    while (token_next_is_operator(",")) {
        token_next();
        struct token* next_name_token = token_next();
        if (next_name_token->type != TOKEN_TYPE_IDENTIFIER) {
            compiler_error(current_process, "Esperado identificador após vírgula\n");
            break;
        }
        // Processa colchetes para arrays
        array_dim_count = 0;
    while (token_next_is_operator("[")) {
        token_next(); // Consome o '['
        parse_expressionable_root(history_down(history, history->flags)); // Analisa a expressão dentro dos colchetes
        struct node* dim_node = node_pop(); // Pega o nó da dimensão

        if (dim_node && dim_node->type == NODE_TYPE_NUMBER) {
            array_dims[array_dim_count++] = dim_node->llnum;
        } else {
            // Caso a dimensão não seja um número (e.g., uma variável ou expressão complexa)
            // Você precisaria armazenar o nó da expressão para avaliação posterior.
            compiler_error(current_process, "Esperado numero inteiro ou expressao constante para tamanho do array\n");
            break;
        }

        array_dims[array_dim_count++] = dim_node->inum;
        expected_sym(']');  // Espera o ']'
    }
        value_node = NULL;
        if (token_next_is_operator("=")) {
            token_next();
            parse_expressionable_root(history);
            value_node = node_pop();
        }

        // Criar node para a variável
        make_variable_node(dtype, next_name_token, value_node);
        var_node = node_pop();
        if (!var_node) {
            compiler_error(current_process, "Falha ao criar node para variável\n");
            break;
        }

        // Armazenar as dimensões do array no node
        if (array_dim_count > 0) {
            var_node->var.type.flags |= DATATYPE_FLAG_IS_ARRAY;
            var_node->var.type.pointer_depth = array_dim_count;
            struct datatype* current_dim_dt = &var_node->var.type;
            for (int i = 0; i < array_dim_count; i++) {
                if (i == 0) {
                    current_dim_dt->size = array_dims[0];
                } else {
                    current_dim_dt->datatype_secondary = calloc(1, sizeof(struct datatype));
                    if (!current_dim_dt->datatype_secondary) {
                        compiler_error(current_process, "Falha ao alocar memoria para datatype secundario de array\n");
                        break;
                    }
                    current_dim_dt = current_dim_dt->datatype_secondary;
                    current_dim_dt->size = array_dims[i];
                    current_dim_dt->type = DATATYPE_INTEGER;
                    current_dim_dt->flags |= DATATYPE_FLAG_IS_SIGNED;
                }
            }
        }
        vector_push(var_list_node.var_list.list, &var_node);
    }
    
    expected_sym(';'); // Verifica se há um ";" no final da declaração
    
    struct node* created_node = node_create(&var_list_node);
    if (!created_node) {
        compiler_error(current_process, "Falha ao criar node da lista de variáveis\n");
        vector_free(var_list_node.var_list.list);
        return;
    }
    for (int i = 0; i < vector_count(var_list_node.var_list.list); i++) {
        struct node** var_ptr = vector_at(var_list_node.var_list.list, i);
        if (!var_ptr || !*var_ptr) continue;
        struct token temp_token = {
            .type = TOKEN_TYPE_IDENTIFIER,
            .sval = (*var_ptr)->var.name,
            .pos = (*var_ptr)->pos
        };
        make_variable_node_and_register(history, &((*var_ptr)->var.type), &temp_token, (*var_ptr)->var.val);
    }
    node_push(created_node);
}

void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node) {
    make_variable_node(dtype, name_token, value_node);
    struct node* var_node = node_pop();
    // 1 - Calcular o escopo offset
    // 2 - Adicionar a variavel no escopo correto
    node_push(var_node); // Empurra de volta para ser parte da lista de variáveis ou para o nó principal
}

void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node) {
    const char* name_str = NULL;
    if (name_token) name_str = name_token->sval;
    struct node* new_var_node = node_create(&(struct node){
        .type = NODE_TYPE_VARIABLE,
        .var.name = name_str,
        .var.type = *dtype, // Copia a estrutura datatype
        .var.val = value_node,
        .pos = name_token->pos
    });
    node_push(new_var_node);
}

// Função para processar structs
void parse_struct(struct history* history) {
    printf("\nProcessando struct:\n");
    
    // Pega o nome da struct
    struct token* struct_name = token_next();
    if (!struct_name || struct_name->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Esperado nome da struct\n");
        return;
    }
    printf("Nome da struct: %s\n", struct_name->sval);
    
    // Verifica se há chave de abertura
    struct token* open_brace = token_next();
    if (!token_is_symbol(open_brace, '{')) {
        compiler_error(current_process, "Esperado '{' após nome da struct\n");
        return;
    }
    
    // Cria lista de membros da struct
    struct vector* members = vector_create(sizeof(struct node*));
    if (!members) {
        compiler_error(current_process, "Falha ao criar lista de membros da struct\n");
        return;
    }
    
    // Processa os membros da struct
    while (true) {
        struct token* next_token = token_peek_next();
        if (!next_token) break;
        
        // Verifica se é o fechamento da struct
        if (token_is_symbol(next_token, '}')) {
            token_next(); // Consome o '}'
            break;
        }
        
        // Processa declaração de membro
        struct datatype member_type;
        parse_datatype(&member_type);
        
        struct token* member_name = token_next();
        if (!member_name || member_name->type != TOKEN_TYPE_IDENTIFIER) {
            compiler_error(current_process, "Esperado nome do membro da struct\n");
            break; // Sair do loop de membros na mesma linha
        }
        
        // Cria node para o membro
        make_variable_node(&member_type, member_name, NULL);
        struct node* member_node = node_pop();
        if (member_node) {
            vector_push(members, &member_node);
            printf("Membro adicionado: %s %s\n", member_type.type_str, member_name->sval);
        }
        
        // Verifica se há mais membros na mesma linha
        while (token_next_is_operator(",")) {
            token_next(); // Consome a vírgula
            
            struct token* next_member = token_next();
            if (next_member->type != TOKEN_TYPE_IDENTIFIER) {
                compiler_error(current_process, "Esperado nome do membro após vírgula\n");
                break;
            }
            
            make_variable_node(&member_type, next_member, NULL);
            member_node = node_pop();
            if (member_node) {
                vector_push(members, &member_node);
                printf("Membro adicionado: %s %s\n", member_type.type_str, next_member->sval);
            }
        }
        
        // Verifica ponto e vírgula
        expected_sym(';');
    }
    
    // Verifica ponto e vírgula após o fechamento da struct
    expected_sym(';');
    
    // Cria o node da struct
    struct node struct_node_data = { // Usar um nome diferente para evitar conflito
        .type = NODE_TYPE_STRUCT,
        .sval = struct_name->sval,
        .pos = struct_name->pos
    };
    
    struct node* created_struct = node_create(&struct_node_data);
    if (!created_struct) {
        compiler_error(current_process, "Falha ao criar node da struct\n");
        vector_free(members); // Libera o vetor de membros mesmo em err
        return;
    }
    
    // Associar os membros à struct (você precisaria adicionar um campo para isso em struct node)
    // Por enquanto, os membros são mantidos no vetor `members`, que precisaria ser gerenciado externamente.
    printf("Struct criada com %d membros (membros armazenados em vector, nao diretamente no no da struct)\n", vector_count(members));
    node_push(created_struct);
}

// Função auxiliar para verificar se um token é parênteses
static bool token_is_parenthesis(struct token* token, char paren) {
    if (!token) return false;

    // C trata '(' e ')' como símbolos, não operadores.
    
    if (token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, &paren)) {
        return true;
    }
    
    if (token->type == TOKEN_TYPE_SYMBOL && token->cval == paren) {
        return true;
    }
    
    
    return false;
}

// Função para processar ifs
void parse_if(struct history* history) {
    printf("\nProcessando if:\n");
    
    // Verifica parênteses de abertura
    if (token_next_is_operator("(") ) {
        // Se o próximo token não for '(', é um erro sintático.
        compiler_error(current_process, "Esperado '(' apos if\n");
        return;
    }
    struct token* open_paren = token_peek_next(); // PEEK para ver o que vem depois de 'if'
    
    token_next(); // Consome o '('
    
    // Processa a condição
    // parse_expressionable_root irá lidar com a expressão 'A = 10'
    parse_expressionable_root(history);
    struct node* condition = node_pop();
    if (!condition) {
        printf("Aviso: Condicao do if nao foi processada corretamente ou esta vazia.\n");
        // Continua mesmo sem condição válida, mas gera aviso.
    } else {
        printf("A condicao do if foi processada. Tipo do no da condicao: ");
        print_node_type(condition);
        printf("\n");
    }
    
    // Verifica parênteses de fechamento
    struct token* close_paren = token_next();
    if (!close_paren || !token_is_parenthesis(close_paren, ')')) {
        compiler_error(current_process, "Esperado ')' apos condicao do if\n");
        return;
    }
    
    // Verifica chave de abertura para o corpo do if
    struct token* open_brace = token_next();
    if (!open_brace || !token_is_symbol(open_brace, '{')) {
        compiler_error(current_process, "Esperado '{' apos condicao do if\n");
        return;
    }
    
    // Processa o corpo do if
    // Em uma implementação real, você teria um nó de bloco (NODE_TYPE_BLOCK)
    // que conteria uma lista de statements. NODE_TYPE_VARIABLE_LIST é usado
    // aqui como um placeholder temporário para essa lista de statements.
    struct node* if_body_statements_list = node_create(&(struct node){.type = NODE_TYPE_VARIABLE_LIST, .var_list.list = vector_create(sizeof(struct node*))});
    if (!if_body_statements_list || !if_body_statements_list->var_list.list) {
        compiler_error(current_process, "Falha ao criar lista de statements do if\n");
        return;
    }

    int body_statement_count = 0;
    while (true) {
        struct token* next_token_in_body = token_peek_next();
        if (!next_token_in_body) break; // EOF
        
        // Verifica se é o fechamento do if
        if (token_is_symbol(next_token_in_body, '}')) {
            token_next(); // Consome o '}'
            break;
        }
        
        // Processa statement dentro do corpo do if
        if (parse_next() == 0) {
            struct node* statement = node_pop(); // Pega o nó do statement que parse_next empurrou
            if (statement) {
                vector_push(if_body_statements_list->var_list.list, &statement);
                printf("Statement adicionado ao corpo do if\n");
                body_statement_count++;
            } else {
                printf("Aviso: parse_next() nao retornou um statement para o corpo do if, mas nao houve erro critico. Avancando token.\n");
                token_next(); // Avanca para evitar loop infinito
            }
        } else {
            // Se parse_next falhou, talvez seja um erro ou um token que não conseguiu ser tratado
            compiler_error(current_process, "Erro ao processar statement no corpo do if. Token atual: Tipo %d, Valor '%s' (ou '%c')\n",
                           next_token_in_body->type, next_token_in_body->sval, next_token_in_body->cval);
            break; // Sair do loop do corpo do if em caso de erro
        }
    }
    
    // Cria o node do if
    struct node if_node_data = { // Usar um nome diferente para evitar conflito
        .type = NODE_TYPE_STATEMENT_IF,
        .pos = open_paren->pos, // Posição do '('
        .exp.left = condition, // Condição do if
        .exp.right = if_body_statements_list // O corpo do if (lista de statements)
    };
    
    struct node* created_if = node_create(&if_node_data);
    if (!created_if) {
        compiler_error(current_process, "Falha ao criar node do if\n");
        vector_free(if_body_statements_list->var_list.list); // Libera o vetor de statements
        free(if_body_statements_list); // Libera o nó placeholder da lista
        return;
    }
    
    printf("If criado com %d statements no corpo.\n", body_statement_count);
    node_push(created_if);
}
