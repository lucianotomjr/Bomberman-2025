// ====================================================================================================================
// Bomberman
// Autores: João Matheus Cachoeira Pimentel, Luciano Tomasi Junior e Luiza Almeida Deon
// ====================================================================================================================

#include <iostream>
#include <windows.h>
#include <conio.h>
#include <ctime>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

using namespace std;

// Constantes
static const int LINHAS    = 15;
static const int COLUNAS   = 40;
static const int NUM_FASES = 3;

// Pontuação
static const double PTS_ITEM        = 0.3;
static const double PTS_CAIXA       = 0.5;
static const double PTS_MATAR_INIM  = 5.0;
static const double PTS_MATAR_CHEFE = 15.0;
static const double PEN_MOV         = 0.01;
static const double PEN_BOMBA       = 0.05;
static const double PEN_CONTINUAR   = 50.0;
static const double BONUS_VITORIA   = 150.0; // bônus por vencer a campanha

// Geração do mapa
static const double PROB_QUEBRAVEL_EM_SOLIDO = 0.40;

// Arquivo de ranking
static const char* ARQ_RANK = "ranking_bomberman.csv";

// "Infinito" simples para BFS
static const int INF = 1000000000;

// RNG simples
int rnd(int a, int b) { return a + (rand() % (b - a + 1)); } // [a..b]
bool prob(int pct)    { return (rand() % 100) < pct; }       // 0..99

// Fila simples para BFS
struct FilaXY {
    int qx[LINHAS * COLUNAS];
    int qy[LINHAS * COLUNAS];
    int ini = 0, fim = 0;
    void reset(){ ini = fim = 0; }
    bool empty()const{ return ini == fim; }
    void push(int x,int y){ qx[fim] = x; qy[fim] = y; ++fim; }
    void pop(int &x,int &y){ x = qx[ini]; y = qy[ini]; ++ini; }
};

// Console / Windows
static HANDLE SAIDA_CONSOLE = GetStdHandle(STD_OUTPUT_HANDLE);
void definirCor(WORD a){ SetConsoleTextAttribute(SAIDA_CONSOLE, a); }
void limparTela(){ system("cls"); }
void pausar(){ system("pause"); }
void ocultarCursor() {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(out, &ci);
    ci.bVisible = false;
    SetConsoleCursorInfo(out, &ci);
}
void reposicionarTopo() {
    static COORD coord = {0,0};
    SetConsoleCursorPosition(SAIDA_CONSOLE, coord);
}
unsigned long long agoraMilissegundos(){ return GetTickCount64(); }

// Cores
const WORD COR_PADRAO   = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE;
const WORD COR_FORTE    = COR_PADRAO|FOREGROUND_INTENSITY;
const WORD COR_CINZA    = FOREGROUND_INTENSITY;
const WORD COR_VERDE    = FOREGROUND_GREEN|FOREGROUND_INTENSITY;
const WORD COR_VERMELHO = FOREGROUND_RED|FOREGROUND_INTENSITY;
const WORD COR_AZUL     = FOREGROUND_BLUE|FOREGROUND_INTENSITY;
const WORD COR_AMARELO  = FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY;
const WORD COR_CIANO    = FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
const WORD COR_MAGENTA  = FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
const WORD COR_CIANO_ESC= FOREGROUND_GREEN|FOREGROUND_BLUE;

const WORD COR_J1       = COR_VERDE;
const WORD COR_J2       = COR_MAGENTA;
const WORD COR_INIMIGO  = COR_VERMELHO;
const WORD COR_CHEFE    = COR_MAGENTA;
const WORD COR_PAREDE   = COR_CINZA;
const WORD COR_QUEBRAVEL= COR_CIANO_ESC;
const WORD COR_BOMBA    = COR_AMARELO;
const WORD COR_RELOGIO  = COR_CIANO;
const WORD COR_EXPLOSAO = COR_AMARELO;
const WORD COR_PORTAL   = COR_AZUL;


// Enums & auxiliares

enum class Bloco : unsigned char { Vazio=0, Solido=1, Quebravel=2 };
enum class Item  : unsigned char { Nenhum=0, Fogo=1, Bombas=2, Vida=3, Relogio=4, Imune=5, Atravessar=6 };

int indice(int i,int j){ return i*COLUNAS + j; }
bool dentro(int i,int j){ return (i>=0 && i<LINHAS && j>=0 && j<COLUNAS); }

constexpr char GLIFO_PAREDE = (char)219;
constexpr char GLIFO_QUEB   = (char)178;
constexpr char GLIFO_BOMBA  = (char)228;
constexpr char GLIFO_EXP    = '*';
constexpr char GLIFO_PORTAL = 'O';
constexpr char GLIFO_CHEFE  = 'X';
constexpr char GLIFO_JOG    = '$';

// utilitário genérico restante
template<int L, int C>
bool dentroT(int i,int j){ return (i>=0 && i<L && j>=0 && j<C); }

string dataHoraAtual(){
    time_t t = time(NULL);
    struct tm tmv{};
#if defined(_MSC_VER)
    localtime_s(&tmv, &t);
#else
    tm* ptm = localtime(&t);
    if(ptm) tmv=*ptm;
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return string(buf);
}
string sanitizarCSV(const string& s){
    string r=s; for(char& c: r){ if(c==','||c=='\n'||c=='\r') c=' '; } return r;
}
string aparar(const string& s){
    size_t a = s.find_first_not_of(" \t\r\n");
    if(a==string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}


// Configuração de dificuldade

struct ConfiguracaoDificuldade {
    int numInimigos;
    int tempoPerseguindo; // ms
    int tempoAleatorio;   // ms
    int raioPerseguicao;
    string nome;
};
int vidasIniciais(const ConfiguracaoDificuldade& cfg){
    if (cfg.nome == "Dificil") return 1;
    if (cfg.nome == "Medio")   return 2;
    return 3;
}


// Estado do jogo

struct Jogador {
    int x=1,y=1;
    int vidas=3;
    int maxBombas=1;
    int bombasAtivas=0;
    int raioFogo=1;
    bool podeAtravessar=false;
    bool imuneExplosao=false;

    int  relogioCargas=0;
    bool relogioColocado=false;
    int  rx=-1, ry=-1;

    int itensPegos[8]={0}; // estatística
};

struct Inimigo {
    int x=1,y=1; bool vivo=true;
    unsigned long long ultimoMov=0;
};

struct Chefe {
    bool vivo=false;
    int x=-1, y=-1;
    int hp=3;
    unsigned long long ultimoMov=0;
    int ultimaOndaAtingida = -1;

    int intervaloMs=120;
    int chanceAleatoria=5;
    bool medoBomba=false; // só no Difícil
};

struct Bomba {
    int x,y;
    int dono;               // 0 = solo, 1 = J1, 2 = J2
    bool relogio=false;     // manual
    unsigned long long momentoPlantio=0;
};

struct CelulaExplosao {
    int x,y;
    unsigned long long expiraEm;
    int onda; // id da onda/detonacao
};

struct Portal {
    bool aberto=false;
    int x=-1,y=-1;
};

struct EstatisticasFase {
    double pontos=0.0;
    int movimentos=0;
    int bombasUsadas=0;
    int caixasQuebradas=0;
    int inimigosMortos=0;
    int chefeMorto=0;
    unsigned long long tempoMs=0;
    void reset(){ *this = EstatisticasFase(); }
    void somar(const EstatisticasFase& o){
        pontos+=o.pontos; movimentos+=o.movimentos; bombasUsadas+=o.bombasUsadas;
        caixasQuebradas+=o.caixasQuebradas; inimigosMortos+=o.inimigosMortos;
        chefeMorto+=o.chefeMorto; tempoMs+=o.tempoMs;
    }
};

struct RegistroRanking {
    string data;
    string jogadores;
    string dificuldade;
    double pontos=0;
    int fases=0;
    int venceu=0;
    int bombas=0;
    int movimentos=0;
    int inimigos=0;
    int caixas=0;
    int itens=0;
    int tempoSeg=0;
};

struct EstadoJogo {
    vector<Bloco> mapa;    // LINHAS*COLUNAS
    vector<Item>  itens;   // LINHAS*COLUNAS

    unsigned char mascaraExpl[LINHAS][COLUNAS] = {{0}};
    int ondaCel[LINHAS][COLUNAS] = {{0}};
    vector<CelulaExplosao> explosoes;
    vector<Bomba> bombas;

    unsigned char tipoBomba[LINHAS][COLUNAS] = {{0}}; // 0 nada, 1 normal, 2 relógio
    unsigned char donoBomba[LINHAS][COLUNAS] = {{0}}; // 0 nada, 1..N
    unsigned long long tempoBomba[LINHAS][COLUNAS] = {{0}};

    vector<Jogador> jogadores;
    vector<Inimigo> inimigos;

    Chefe chefe;
    Portal portal;

    ConfiguracaoDificuldade cfg;
    int fase=1;
    EstatisticasFase est;

    int proximaOndaId = 1;
};


// Layout base

static const int MAPA_BASE[LINHAS][COLUNAS] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,1,0,0,0,0,2,0,0,0,0,0,0,0,0,0,2,0,0,0,1,
    1,0,1,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1,1,0,1,0,1,1,0,1,1,2,1,0,1,1,1,0,1,1,1,1,0,1,
    1,0,1,0,0,0,1,0,0,0,1,0,2,0,0,0,0,1,0,0,1,0,1,0,0,1,0,0,1,2,0,0,1,0,1,0,0,1,0,1,
    1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,0,1,1,0,1,0,1,
    1,0,0,0,0,2,0,0,1,0,0,0,1,0,0,0,0,1,0,2,0,0,1,0,0,0,0,1,0,0,1,0,0,2,0,0,0,1,0,1,
    1,2,1,1,1,0,1,1,1,2,1,0,1,1,1,1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,1,2,1,0,1,1,1,1,0,1,
    1,0,1,0,0,0,1,0,0,0,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,2,0,0,0,0,0,1,0,1,0,0,0,2,1,
    1,0,1,0,1,1,1,0,1,0,2,0,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,0,1,1,0,1,0,1,
    1,0,0,0,0,0,0,0,1,0,2,0,1,0,0,2,0,1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0,0,0,0,1,0,1,
    1,0,1,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1,1,1,1,0,1,1,2,1,1,2,1,1,1,1,1,0,1,1,1,1,0,1,
    1,0,1,0,0,0,1,0,0,0,1,0,1,2,0,0,0,1,0,0,1,0,1,0,0,1,0,0,0,0,0,0,1,0,1,0,0,1,0,1,
    1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,1,2,1,0,1,1,0,1,0,1,1,2,1,1,0,1,0,1,0,1,1,0,1,0,1,
    1,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

int celulaBaseFase(int fase,int i,int j){
    if(fase==1) return MAPA_BASE[i][j];
    if(fase==2) return MAPA_BASE[i][COLUNAS-1-j];
    return MAPA_BASE[LINHAS-1-i][j];
}

// Áreas seguras
const int CEL_SEG_J1[][2] = { {1,1},{1,2},{1,3},{2,1},{3,1},{2,2} };
const int QTD_CEL_SEG_J1 = sizeof(CEL_SEG_J1)/sizeof(CEL_SEG_J1[0]);
const int CEL_SEG_J2[][2] = { {1,COLUNAS-2},{1,COLUNAS-3},{2,COLUNAS-2},{3,COLUNAS-2},{2,COLUNAS-3} };
const int QTD_CEL_SEG_J2 = sizeof(CEL_SEG_J2)/sizeof(CEL_SEG_J2[0]);


// SPAWNS FIXOS (inimigos e chefe)

static const int ENEMY_SPAWNS_F1[][2] = {
    {3,5},{3,10},{3,20},{5,7},{5,25},{7,10},
    {7,30},{9,5},{9,20},{11,15},{11,28},{12,35}
};
static const int ENEMY_SPAWNS_F2[][2] = {
    {3,30},{3,25},{3,15},{5,32},{5,10},{7,25},
    {7,5},{9,34},{9,15},{11,20},{11,7},{12,4}
};
static const int ENEMY_SPAWNS_F3[][2] = {
    {2,20},{4,8},{4,30},{6,18},{6,26},{8,12},
    {8,22},{10,35},{10,5},{12,18},{12,26},{13,10}
};
static const int ENEMY_SPAWNS_F1_COUNT = sizeof(ENEMY_SPAWNS_F1)/sizeof(ENEMY_SPAWNS_F1[0]);
static const int ENEMY_SPAWNS_F2_COUNT = sizeof(ENEMY_SPAWNS_F2)/sizeof(ENEMY_SPAWNS_F2[0]);
static const int ENEMY_SPAWNS_F3_COUNT = sizeof(ENEMY_SPAWNS_F3)/sizeof(ENEMY_SPAWNS_F3[0]);

// posição do chefe por fase (o chefe só aparece na fase 3, mas deixamos definido para organização)
static const int CHEFE_SPAWN[NUM_FASES][2] = {
    {7, 10},  // Fase 1 (não é usado)
    {7, 28},  // Fase 2 (não é usado)
    {7, 20}   // Fase 3 (USADO)
};


// Itens

char caractereDoItem(Item it){
    switch(it){
        case Item::Fogo:       return 'F';
        case Item::Bombas:     return 'B';
        case Item::Vida:       return '+';
        case Item::Relogio:    return 'R';
        case Item::Imune:      return 'I';
        case Item::Atravessar: return 'A';
        default: return ' ';
    }
}
WORD corDoItem(Item it){
    switch(it){
        case Item::Fogo:       return COR_VERMELHO;
        case Item::Bombas:     return COR_AMARELO;
        case Item::Vida:       return COR_FORTE;
        case Item::Relogio:    return COR_CIANO;
        case Item::Imune:      return COR_MAGENTA;
        case Item::Atravessar: return COR_VERDE;
        default: return COR_PADRAO;
    }
}
const char* nomeDoItem(Item it){
    switch(it){
        case Item::Fogo:       return "Fogo + (Raio)";
        case Item::Bombas:     return "Bombas +";
        case Item::Vida:       return "Vida +";
        case Item::Relogio:    return "Relogio";
        case Item::Imune:      return "Imunidade";
        case Item::Atravessar: return "Atravessar quebraveis";
        default: return "";
    }
}
void tentarDroparItem(EstadoJogo& E,int i,int j){
    if (prob(70)) {
        E.itens[indice(i,j)] = static_cast<Item>(rnd(1,6)); // 1..6
    }
}

// Sobrecarga: usa o acumulador do próprio EstadoJogo
void coletarItem(Jogador& J, EstadoJogo& E, double& pontos){
    Item it = E.itens[indice(J.x,J.y)];
    if(it==Item::Nenhum) return;
    switch(it){
        case Item::Fogo:       J.raioFogo++;       J.itensPegos[(int)Item::Fogo]++; break;
        case Item::Bombas:     J.maxBombas++;      J.itensPegos[(int)Item::Bombas]++; break;
        case Item::Vida:       J.vidas++;          J.itensPegos[(int)Item::Vida]++; break;
        case Item::Relogio:    J.relogioCargas++;  break; // HUD mostra cargas
        case Item::Imune:      J.imuneExplosao=true; break; // HUD mostra ATIVA
        case Item::Atravessar: J.podeAtravessar=true; J.itensPegos[(int)Item::Atravessar]++; break;
        default: break;
    }
    pontos += PTS_ITEM;
    E.itens[indice(J.x,J.y)] = Item::Nenhum;
}
void coletarItem(Jogador& J, EstadoJogo& E){
    coletarItem(J, E, E.est.pontos);
}


// Mapa / Fase

void gerarMapaDaFase(EstadoJogo& E){
    E.mapa.assign(LINHAS*COLUNAS, Bloco::Vazio);
    E.itens.assign(LINHAS*COLUNAS, Item::Nenhum);

    for(int i=0;i<LINHAS;i++){
        for(int j=0;j<COLUNAS;j++){
            if(i==0||j==0||i==LINHAS-1||j==COLUNAS-1){
                E.mapa[indice(i,j)] = Bloco::Solido; continue;
            }
            int base = celulaBaseFase(E.fase,i,j);
            if(base==1){
                double r = (double)rand()/RAND_MAX;
                E.mapa[indice(i,j)] = (r < PROB_QUEBRAVEL_EM_SOLIDO)? Bloco::Quebravel : Bloco::Solido;
            }else E.mapa[indice(i,j)] = Bloco::Vazio;
        }
    }
    // Áreas seguras
    for(int k=0;k<QTD_CEL_SEG_J1;k++){ E.mapa[indice(CEL_SEG_J1[k][0],CEL_SEG_J1[k][1])] = Bloco::Vazio; }
    for(int k=0;k<QTD_CEL_SEG_J2;k++){ E.mapa[indice(CEL_SEG_J2[k][0],CEL_SEG_J2[k][1])] = Bloco::Vazio; }

    // Garante pelo menos um vizinho aberto
    auto abrirVizinho=[&](int i,int j){
        int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
        int opcoes[4][2], qt=0;
        for(int d=0; d<4; d++){
            int ni=i+di[d], nj=j+dj[d];
            if(!dentro(ni,nj)) continue;
            if(E.mapa[indice(ni,nj)]==Bloco::Quebravel){ opcoes[qt][0]=ni; opcoes[qt][1]=nj; qt++; }
        }
        if(qt>0){
            int p=rnd(0,qt-1);
            E.mapa[indice(opcoes[p][0],opcoes[p][1])] = Bloco::Vazio;
        }
    };
    for(int i=1;i<LINHAS-1;i++){
        for(int j=1;j<COLUNAS-1;j++){
            if(E.mapa[indice(i,j)]==Bloco::Vazio){
                int vaz = (E.mapa[indice(i-1,j)]==Bloco::Vazio) + (E.mapa[indice(i+1,j)]==Bloco::Vazio)
                        + (E.mapa[indice(i,j-1)]==Bloco::Vazio) + (E.mapa[indice(i,j+1)]==Bloco::Vazio);
                if(vaz==0) abrirVizinho(i,j);
            }
        }
    }

    // limpa estado volátil
    for (int i = 0; i < LINHAS; ++i) {
        for (int j = 0; j < COLUNAS; ++j) {
            E.mascaraExpl[i][j] = 0;
            E.ondaCel[i][j]     = 0;
            E.tipoBomba[i][j]   = 0;
            E.donoBomba[i][j]   = 0;
            E.tempoBomba[i][j]  = 0;
        }
    }
    E.bombas.clear();
    E.explosoes.clear();

    E.portal = Portal{};
    E.proximaOndaId = 1;
}


// HUD / Render

void imprimirTempo(unsigned long long ms){
    unsigned long long s=ms/1000; int mm=(int)(s/60ULL), ss=(int)(s%60ULL);
    cout<<setw(2)<<setfill('0')<<mm<<":"<<setw(2)<<ss<<setfill(' ');
}

void imprimirLinhaItens(const Jogador& J){
    definirCor(COR_PADRAO); cout<<"Itens: ";

    definirCor(corDoItem(Item::Fogo));
    cout<<nomeDoItem(Item::Fogo)<<": "<<J.itensPegos[(int)Item::Fogo]<<" ";
    definirCor(COR_PADRAO); cout<<"| ";

    definirCor(corDoItem(Item::Bombas));
    cout<<nomeDoItem(Item::Bombas)<<": "<<J.itensPegos[(int)Item::Bombas]<<" ";
    definirCor(COR_PADRAO); cout<<"| ";

    definirCor(corDoItem(Item::Vida));
    cout<<nomeDoItem(Item::Vida)<<": "<<J.itensPegos[(int)Item::Vida]<<" ";
    definirCor(COR_PADRAO); cout<<"| ";

    definirCor(corDoItem(Item::Relogio));
    cout<<nomeDoItem(Item::Relogio)<<": "<<J.relogioCargas<<" ";
    definirCor(COR_PADRAO); cout<<"| ";

    definirCor(corDoItem(Item::Imune));
    cout<<"Imunidade: "<<(J.imuneExplosao ? "ATIVA" : "-");
    definirCor(COR_PADRAO); cout<<" | ";

    definirCor(corDoItem(Item::Atravessar));
    cout<<"Atravessar: "<<(J.podeAtravessar ? "ATIVO" : "-");
    definirCor(COR_PADRAO); cout<<"\n";
}

void imprimirHUD(const EstadoJogo& E, unsigned long long tIni){
    int vivos=0; for(const auto& inim:E.inimigos) if(inim.vivo) vivos++; if(E.chefe.vivo) vivos++;

    definirCor(COR_FORTE);
    cout<<"Fase "<<E.fase<<"/"<<NUM_FASES<<" | Dificuldade: "<<E.cfg.nome<<" | Tempo: ";
    imprimirTempo(agoraMilissegundos()-tIni);
    cout<<" | Pontos: "<<fixed<<setprecision(1)<<E.est.pontos<<"\n";

    if(E.jogadores.size()==1){
        const auto& J=E.jogadores[0];
        definirCor(COR_CIANO);
        cout<<"Vidas:"<<J.vidas<<" | Bombas:"<<J.bombasAtivas<<"/"<<J.maxBombas
            <<" | Raio:"<<J.raioFogo
            <<" | Mov:"<<E.est.movimentos<<" | Bombas usadas:"<<E.est.bombasUsadas
            <<" | Inimigos vivos:"<<vivos;
        definirCor(COR_PADRAO);
        cout<<" | Portal: "; definirCor(E.portal.aberto?COR_PORTAL:COR_CINZA); cout<<(E.portal.aberto?"ABERTO":"FECHADO"); definirCor(COR_PADRAO);
        if(E.chefe.vivo || E.chefe.hp<=0){
            cout<<" | Chefe: "; definirCor(E.chefe.vivo?COR_CHEFE:COR_CINZA); cout<<(E.chefe.vivo?("HP="+to_string(E.chefe.hp)):"MORTO"); definirCor(COR_PADRAO);
        }
        cout<<"\n"; imprimirLinhaItens(J);
    }else{
        const auto& J1=E.jogadores[0];
        const auto& J2=E.jogadores[1];

        definirCor(COR_J1);
        cout<<"J1 Vidas:"<<J1.vidas<<(J1.vidas<=0?" (K.O.)":"")
            <<" | Bombas:"<<J1.bombasAtivas<<"/"<<J1.maxBombas
            <<" | Raio:"<<J1.raioFogo
            <<(J1.relogioColocado?" | Relogio ARMADO":"")
            <<(J1.relogioCargas>0?(" | Cargas:"+to_string(J1.relogioCargas)):"")
            <<(J1.imuneExplosao?" | Imune":"")
            <<(J1.podeAtravessar?" | Atravessar":"");
        definirCor(COR_PADRAO); cout<<"\n"; imprimirLinhaItens(J1);

        definirCor(COR_J2);
        cout<<"J2 Vidas:"<<J2.vidas<<(J2.vidas<=0?" (K.O.)":"")
            <<" | Bombas:"<<J2.bombasAtivas<<"/"<<J2.maxBombas
            <<" | Raio:"<<J2.raioFogo
            <<(J2.relogioColocado?" | Relogio ARMADO":"")
            <<(J2.relogioCargas>0?(" | Cargas:"+to_string(J2.relogioCargas)):"")
            <<(J2.imuneExplosao?" | Imune":"")
            <<(J2.podeAtravessar?" | Atravessar":"");
        definirCor(COR_PADRAO); cout<<"\n"; imprimirLinhaItens(J2);

        definirCor(COR_CIANO);
        cout<<"Movimentos:"<<E.est.movimentos<<" | Bombas usadas:"<<E.est.bombasUsadas
            <<" | Inimigos vivos:"<<vivos;
        definirCor(COR_PADRAO);
        cout<<" | Portal: "; definirCor(E.portal.aberto?COR_PORTAL:COR_CINZA); cout<<(E.portal.aberto?"ABERTO":"FECHADO"); definirCor(COR_PADRAO);
        if(E.chefe.vivo || E.chefe.hp<=0){
            cout<<" | Chefe: "; definirCor(E.chefe.vivo?COR_CHEFE:COR_CINZA); cout<<(E.chefe.vivo?("HP="+to_string(E.chefe.hp)):"MORTO"); definirCor(COR_PADRAO);
        }
        cout<<"\n";
    }
}

void imprimirCharCor(char ch, WORD cor, WORD &cur){ if(cor!=cur){ definirCor(cor); cur=cor; } cout<<ch; }

void imprimirMapa(const EstadoJogo& E){
    WORD cur=COR_PADRAO; definirCor(cur);
    for(int i=0;i<LINHAS;i++){
        for(int j=0;j<COLUNAS;j++){
            if(E.chefe.vivo && i==E.chefe.x && j==E.chefe.y){ imprimirCharCor(GLIFO_CHEFE,COR_CHEFE,cur); continue; }
            bool desen=false;
            for(const auto& m:E.inimigos){
                if(m.vivo && m.x==i && m.y==j){ imprimirCharCor('@',COR_INIMIGO,cur); desen=true; break; }
            }
            if(desen) continue;
            bool tinhaJog=false;
            for(size_t p=0;p<E.jogadores.size();++p){
                const auto& J=E.jogadores[p];
                if(J.vidas>0 && J.x==i && J.y==j){
                    imprimirCharCor(GLIFO_JOG,(p==0?COR_J1:COR_J2),cur); tinhaJog=true; break;
                }
            }
            if(tinhaJog) continue;
            if(E.mascaraExpl[i][j]){ imprimirCharCor(GLIFO_EXP,COR_EXPLOSAO,cur); continue; }
            if(E.tipoBomba[i][j]==1){ imprimirCharCor(GLIFO_BOMBA,COR_BOMBA,cur); continue; }
            if(E.tipoBomba[i][j]==2){ imprimirCharCor('R',COR_RELOGIO,cur); continue; }
            if(E.portal.aberto && i==E.portal.x && j==E.portal.y){ imprimirCharCor(GLIFO_PORTAL,COR_PORTAL,cur); continue; }
            Bloco b = E.mapa[indice(i,j)];
            if(b==Bloco::Solido) imprimirCharCor(GLIFO_PAREDE,COR_PAREDE,cur);
            else if(b==Bloco::Quebravel) imprimirCharCor(GLIFO_QUEB,COR_QUEBRAVEL,cur);
            else{
                Item it = E.itens[indice(i,j)];
                if(it!=Item::Nenhum) imprimirCharCor(caractereDoItem(it), corDoItem(it), cur);
                else imprimirCharCor(' ',COR_PADRAO,cur);
            }
        }
        imprimirCharCor('\n',COR_PADRAO,cur);
    }
}


// Bombas / Explosões (onda por detonação)

void ligarCelulaExplosao(EstadoJogo& E,int i,int j,unsigned long long expira,int onda){
    if(!dentro(i,j)) return;
    if(!E.mascaraExpl[i][j]){
        E.mascaraExpl[i][j]=1;
        E.ondaCel[i][j]=onda;
        E.explosoes.push_back({i,j,expira,onda});
    }else{
        E.ondaCel[i][j]=onda;
    }
}

// RECURSIVIDADE
void expandirExplosaoRec(EstadoJogo& E, int x, int y, int dx, int dy, int alcance,
                         unsigned long long expira, int onda){
    if(alcance<=0) return;
    int nx = x + dx;
    int ny = y + dy;
    if(!dentroT<LINHAS,COLUNAS>(nx,ny)) return;

    Bloco b = E.mapa[indice(nx,ny)];
    if(b==Bloco::Solido) return;

    if(b==Bloco::Quebravel){
        E.mapa[indice(nx,ny)] = Bloco::Vazio;
        tentarDroparItem(E,nx,ny);
        E.est.caixasQuebradas++;
        E.est.pontos += PTS_CAIXA;
        ligarCelulaExplosao(E,nx,ny,expira,onda);
        expandirExplosaoRec(E, nx, ny, dx, dy, alcance-1, expira, onda);
        return;
    }

    ligarCelulaExplosao(E,nx,ny,expira,onda);
    expandirExplosaoRec(E, nx, ny, dx, dy, alcance-1, expira, onda);
}

void propagarExplosao(EstadoJogo& E,int ci,int cj,const Jogador& dono){
    const int ondaId = E.proximaOndaId++;
    unsigned long long expira = agoraMilissegundos() + 500;

    ligarCelulaExplosao(E,ci,cj,expira,ondaId);

    expandirExplosaoRec(E, ci, cj, -1,  0, dono.raioFogo, expira, ondaId);
    expandirExplosaoRec(E, ci, cj,  1,  0, dono.raioFogo, expira, ondaId);
    expandirExplosaoRec(E, ci, cj,  0, -1, dono.raioFogo, expira, ondaId);
    expandirExplosaoRec(E, ci, cj,  0,  1, dono.raioFogo, expira, ondaId);
}

void detonarBomba(EstadoJogo& E, const Bomba& b, const Jogador& dono){
    E.tipoBomba[b.x][b.y]=0; E.donoBomba[b.x][b.y]=0; E.tempoBomba[b.x][b.y]=0;
    propagarExplosao(E,b.x,b.y,dono);
}

// ASSINATURA ÚNICA (DEFAULT)
void plantarBomba(EstadoJogo& E, int px, int py, int idDono, Jogador& J, bool relogio = false) {
    if (E.tipoBomba[px][py] != 0) return;
    if (J.bombasAtivas >= J.maxBombas && !relogio) return;

    E.tipoBomba[px][py]  = (relogio ? 2 : 1);
    E.donoBomba[px][py]  = idDono;
    E.tempoBomba[px][py] = agoraMilissegundos();
    E.bombas.push_back({px, py, idDono, relogio, E.tempoBomba[px][py]});

    if (!relogio) J.bombasAtivas++;
    E.est.bombasUsadas++;
}

void atualizarBombas(EstadoJogo& E){
    unsigned long long agora = agoraMilissegundos();
    vector<Bomba> proximo;
    proximo.reserve(E.bombas.size());
    for(const auto& b: E.bombas){
        bool detona = false;
        if(b.relogio){
            proximo.push_back(b);
            continue;
        }else{
            if(agora - b.momentoPlantio >= 1000) detona = true;
        }
        if(detona){
            int id = b.dono;
            const Jogador* dono = nullptr;
            if(E.jogadores.size()==1){ dono = &E.jogadores[0]; }
            else{
                if(id>=1 && id <= (int)E.jogadores.size()) dono = &E.jogadores[id-1];
                else dono = &E.jogadores[0];
            }
            detonarBomba(E, b, *dono);
            if(!b.relogio && id>=1 && id <= (int)E.jogadores.size()){
                if(E.jogadores[id-1].bombasAtivas>0) E.jogadores[id-1].bombasAtivas--;
            }else if(E.jogadores.size()==1){
                if(E.jogadores[0].bombasAtivas>0) E.jogadores[0].bombasAtivas--;
            }
        }else{
            proximo.push_back(b);
        }
    }
    E.bombas.swap(proximo);
}

void detonarRelogioSeArmado(EstadoJogo& E, Jogador& J){
    if(!J.relogioColocado) return;
    int x=J.rx, y=J.ry;
    if(!dentro(x,y)) return;
    for(size_t i=0;i<E.bombas.size();++i){
        const auto& b=E.bombas[i];
        if(b.relogio && b.x==x && b.y==y && E.tipoBomba[x][y]==2){
            detonarBomba(E,b,J);
            E.bombas.erase(E.bombas.begin()+i);
            break;
        }
    }
    E.tipoBomba[x][y]=0; E.donoBomba[x][y]=0; E.tempoBomba[x][y]=0;
    J.relogioColocado=false; J.rx=J.ry=-1;
    if(J.relogioCargas>0) J.relogioCargas--;
}

void atualizarExplosoes(EstadoJogo& E){
    unsigned long long agora=agoraMilissegundos();
    vector<CelulaExplosao> proximo; proximo.reserve(E.explosoes.size());
    for(const auto& e: E.explosoes){
        if(agora >= e.expiraEm){
            if(E.mascaraExpl[e.x][e.y]){ E.mascaraExpl[e.x][e.y]=0; E.ondaCel[e.x][e.y]=0; }
        }else proximo.push_back(e);
    }
    E.explosoes.swap(proximo);
}


// IA Inimigos / Chefe

bool pertoDeBombaOuExplosao(const EstadoJogo& E,int i,int j,int dist){
    if(dist<=0) return false;
    for(int di=-dist; di<=dist; ++di){
        for(int dj=-dist; dj<=dist; ++dj){
            if(abs(di)+abs(dj)>dist) continue;
            int ni=i+di, nj=j+dj;
            if(!dentro(ni,nj)) continue;
            if(E.tipoBomba[ni][nj]!=0 || E.mascaraExpl[ni][nj]) return true;
        }
    }
    return false;
}

void moverInimigos(EstadoJogo& E){
    unsigned long long agora=agoraMilissegundos();
    for(auto& I: E.inimigos){
        if(!I.vivo) continue;

        int ax=-1, ay=-1; int melhor=INF;
        for(const auto& J:E.jogadores){
            if(J.vidas<=0) continue;
            int d = abs(J.x-I.x)+abs(J.y-I.y);
            if(d<melhor){ melhor=d; ax=J.x; ay=J.y; }
        }
        bool perseguindo = (melhor<=E.cfg.raioPerseguicao);
        int intervalo = perseguindo? E.cfg.tempoPerseguindo : E.cfg.tempoAleatorio;
        if(agora - I.ultimoMov < (unsigned long long)intervalo) continue;
        I.ultimoMov=agora;

        int dir;
        if(perseguindo){
            if(abs(ax-I.x)>abs(ay-I.y)) dir=(ax<I.x)?0:1;
            else dir=(ay<I.y)?2:3;
        }else{
            dir = rnd(0,3);
        }
        int nx=I.x, ny=I.y;
        if(dir==0) nx--; else if(dir==1) nx++; else if(dir==2) ny--; else ny++;
        if(dentro(nx,ny) && E.mapa[indice(nx,ny)]==Bloco::Vazio){ I.x=nx; I.y=ny; }
    }
}

//spawns fixos
static void limparCelulaParaSpawn(EstadoJogo& E, int x, int y){
    if(!dentro(x,y)) return;
    E.mapa[indice(x,y)]  = Bloco::Vazio;
    E.itens[indice(x,y)] = Item::Nenhum;
}

void aplicarSpawnsFixos(EstadoJogo& E){
    const int (*lista)[2] = nullptr;
    int total=0;

    if(E.fase==1){ lista = ENEMY_SPAWNS_F1; total = ENEMY_SPAWNS_F1_COUNT; }
    else if(E.fase==2){ lista = ENEMY_SPAWNS_F2; total = ENEMY_SPAWNS_F2_COUNT; }
    else { lista = ENEMY_SPAWNS_F3; total = ENEMY_SPAWNS_F3_COUNT; }

    // Limpa as células de spawn
    for(int i=0;i<total;i++){
        limparCelulaParaSpawn(E, lista[i][0], lista[i][1]);
    }

    // Preenche inimigos nos primeiros N spawns conforme dificuldade
    int N = min(E.cfg.numInimigos, total);
    E.inimigos.clear();
    E.inimigos.resize(N);
    for(int i=0;i<N;i++){
        E.inimigos[i].x = lista[i][0];
        E.inimigos[i].y = lista[i][1];
        E.inimigos[i].vivo = true;
        E.inimigos[i].ultimoMov = 0;
    }
}

//chefe agora nasce em posição fixa
void inicializarChefe(EstadoJogo& E){
    Chefe C{};
    C.vivo = (E.fase==NUM_FASES);
    C.hp = 3;
    C.ultimaOndaAtingida = -1;

    if(E.cfg.nome=="Facil"){
        C.intervaloMs = E.cfg.tempoPerseguindo + 120;
        C.chanceAleatoria = 0;
        C.medoBomba = false;
    }
    else if(E.cfg.nome=="Medio"){
        C.intervaloMs = E.cfg.tempoPerseguindo + 60;
        C.chanceAleatoria = 5;
        C.medoBomba = false;
    }
    else { // Dificil
        C.intervaloMs = E.cfg.tempoPerseguindo;
        C.chanceAleatoria = 5;
        C.medoBomba = true;
    }

    // posição fixa por fase (efetivamente usada só na fase 3)
    int cx = CHEFE_SPAWN[E.fase-1][0];
    int cy = CHEFE_SPAWN[E.fase-1][1];
    if(dentro(cx,cy)){
        limparCelulaParaSpawn(E, cx, cy); // garante vazio
        C.x = cx; C.y = cy;
    }else{
        // fallback robusto (borda segura)
        C.x = LINHAS-2; C.y = COLUNAS-2;
        limparCelulaParaSpawn(E, C.x, C.y);
    }

    E.chefe=C;
}

void moverChefe(EstadoJogo& E){
    if(!E.chefe.vivo) return;

    unsigned long long agora=agoraMilissegundos();
    if(agora - E.chefe.ultimoMov < (unsigned long long)E.chefe.intervaloMs) return;
    E.chefe.ultimoMov = agora;

    int tx=-1, ty=-1, best=INF;
    for(const auto& J: E.jogadores){
        if(J.vidas<=0) continue;
        int d = abs(J.x - E.chefe.x) + abs(J.y - E.chefe.y);
        if(d < best){ best = d; tx = J.x; ty = J.y; }
    }
    if(tx==-1) return;

    auto pode = [&](int i,int j)->bool{
        if(!dentro(i,j)) return false;
        if(E.mapa[indice(i,j)]==Bloco::Solido) return false;
        if(E.mascaraExpl[i][j]) return false;
        if(E.chefe.medoBomba && E.tipoBomba[i][j]!=0) return false;
        return true;
    };

    if(best == 1 && pode(tx,ty)){
        E.chefe.x = tx;
        E.chefe.y = ty;
        return;
    }

    static int dist[LINHAS][COLUNAS];
    for(int i=0;i<LINHAS;i++) for(int j=0;j<COLUNAS;j++) dist[i][j]=INF;

    FilaXY q; q.reset();
    dist[tx][ty]=0; q.push(tx,ty);

    const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
    while(!q.empty()){
        int cx, cy; q.pop(cx,cy);
        for(int k=0;k<4;k++){
            int nx=cx+di[k], ny=cy+dj[k];
            if(!pode(nx,ny)) continue;
            if(dist[nx][ny]!=INF) continue;
            dist[nx][ny] = dist[cx][cy] + 1;
            q.push(nx,ny);
        }
    }

    if(dist[E.chefe.x][E.chefe.y] != INF){
        int bestX = E.chefe.x, bestY = E.chefe.y, bestD = dist[E.chefe.x][E.chefe.y];

        for(int k=0;k<4;k++){
            int nx = E.chefe.x + di[k], ny = E.chefe.y + dj[k];
            if(!pode(nx,ny)) continue;
            if(E.chefe.medoBomba && pertoDeBombaOuExplosao(E, nx, ny, 1)) continue;

            if(dist[nx][ny] < bestD){
                bestD = dist[nx][ny];
                bestX = nx; bestY = ny;
            }
        }

        if(bestX!=E.chefe.x || bestY!=E.chefe.y){
            E.chefe.x = bestX; E.chefe.y = bestY;
            return;
        }
    }

    {
        int bx = E.chefe.x, by = E.chefe.y;
        int melhorScore = INF;
        const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
        for(int k=0;k<4;k++){
            int nx = E.chefe.x + di[k], ny = E.chefe.y + dj[k];
            if(!pode(nx,ny)) continue;
            if(E.chefe.medoBomba && pertoDeBombaOuExplosao(E, nx, ny, 1)) continue;

            int score = abs(tx - nx) + abs(ty - ny);
            if(score < melhorScore){
                melhorScore = score; bx = nx; by = ny;
            }
        }
        if(bx!=E.chefe.x || by!=E.chefe.y){
            E.chefe.x = bx; E.chefe.y = by;
            return;
        }
    }

    if(prob(E.chefe.chanceAleatoria)){
        int op[4][2], qt=0;
        const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
        for(int k=0;k<4;k++){
            int nx = E.chefe.x + di[k], ny = E.chefe.y + dj[k];
            if(!pode(nx,ny)) continue;
            if(E.chefe.medoBomba && pertoDeBombaOuExplosao(E, nx, ny, 1)) continue;
            op[qt][0]=nx; op[qt][1]=ny; qt++;
        }
        if(qt>0){
            int r = rnd(0,qt-1);
            E.chefe.x = op[r][0]; E.chefe.y = op[r][1];
        }
    }
}


// Dano
int aplicarDano(Jogador& J, bool porExplosao){
    if(porExplosao && J.imuneExplosao) return 0;
    J.vidas--;
    if(J.vidas<=0) return 2;
    return 1;
}

// Portal
Portal abrirPortalEmVazio(EstadoJogo& E){
    Portal p{};
    for(int t=0;t<500;t++){
        int x=rnd(1,LINHAS-2), y=rnd(1,COLUNAS-2);
        if(E.mapa[indice(x,y)]==Bloco::Vazio && !E.tipoBomba[x][y] && !E.mascaraExpl[x][y]){
            p={true,x,y}; E.itens[indice(x,y)]=Item::Nenhum; return p;
        }
    }
    p={true,1,3}; E.itens[indice(1,3)]=Item::Nenhum; return p;
}

// Entrada (teclado)
int lerTecla(){ int t=getch(); if(t==0||t==224){ int t2=getch(); return t2; } return t; }
const int TECLA_CIMA=72, TECLA_BAIXO=80, TECLA_ESQ=75, TECLA_DIR=77;

// Jogo - execução de uma fase

void prepararSpawnsFixos(EstadoJogo& E){
    // garante que as células reservadas para spawns estão livres já no início da fase
    const int (*lista)[2] = nullptr; int total=0;
    if(E.fase==1){ lista = ENEMY_SPAWNS_F1; total = ENEMY_SPAWNS_F1_COUNT; }
    else if(E.fase==2){ lista = ENEMY_SPAWNS_F2; total = ENEMY_SPAWNS_F2_COUNT; }
    else { lista = ENEMY_SPAWNS_F3; total = ENEMY_SPAWNS_F3_COUNT; }
    for(int i=0;i<total;i++){ limparCelulaParaSpawn(E, lista[i][0], lista[i][1]); }
    // chefe (independente de ter chefe ou não nesta fase)
    limparCelulaParaSpawn(E, CHEFE_SPAWN[E.fase-1][0], CHEFE_SPAWN[E.fase-1][1]);
}

bool jogarFase(EstadoJogo& E){
    E.est.reset();
    ocultarCursor(); limparTela();
    definirCor(COR_FORTE);
    cout<<"============================="<<"\n";
    cout<<"         FASE "<<E.fase<<"/"<<NUM_FASES<<(E.jogadores.size()==2?"  (COOP)":"")<<"\n";
    cout<<"============================="<<"\n"; definirCor(COR_PADRAO);
    if(E.fase==NUM_FASES){ definirCor(COR_CHEFE); cout<<"CHEFE: atravessa quebraveis e precisa de 3 bombas!\n"; definirCor(COR_PADRAO); }
    cout<<"Portal aparece apos eliminar todos. Entre no 'O' para avancar.\n";
    cout<<(E.jogadores.size()==1?
        "Solo: WASD/B/R (ou setas para mover)\n" :
        "Coop: J1 (WASD/B/R)  |  J2 (Setas/N/M)\n");
    cout<<"Pressione qualquer tecla...\n"; getch(); limparTela();

    gerarMapaDaFase(E);
    prepararSpawnsFixos(E); // garante que os locais de spawn estão limpos

    if(E.jogadores.size()==1){
        E.jogadores[0].x=1; E.jogadores[0].y=1;
    }else{
        E.jogadores[0].x=1; E.jogadores[0].y=1;
        E.jogadores[1].x=1; E.jogadores[1].y=COLUNAS-2;
    }

    // inimigos em posições fixas
    aplicarSpawnsFixos(E);

    // chefe em posição fixa (só aparece na Fase 3)
    inicializarChefe(E);

    unsigned long long inicio=agoraMilissegundos();

    while(true){
        reposicionarTopo();
        imprimirHUD(E,inicio);
        imprimirMapa(E);

        // Entrada
        if(_kbhit()){
            int t = lerTecla();
            const bool coop = (E.jogadores.size() >= 2);

            // J1
            if(E.jogadores.size()>=1 && E.jogadores[0].vidas>0){
                auto& J=E.jogadores[0];
                int nx=J.x, ny=J.y;

                if((!coop && (t=='w' || t==TECLA_CIMA)) || (coop && t=='w')) nx--;
                else if((!coop && (t=='s' || t==TECLA_BAIXO)) || (coop && t=='s')) nx++;
                else if((!coop && (t=='a' || t==TECLA_ESQ)) || (coop && t=='a')) ny--;
                else if((!coop && (t=='d' || t==TECLA_DIR)) || (coop && t=='d')) ny++;

                else if(t=='b'||t=='B'){
                    plantarBomba(E,J.x,J.y, (E.jogadores.size()==1?0:1), J);
                }else if(t=='r'||t=='R'){
                    if(!J.relogioColocado && J.relogioCargas>0){
                        if(!E.tipoBomba[J.x][J.y]){
                            plantarBomba(E,J.x,J.y,(E.jogadores.size()==1?0:1), J, true);
                            J.relogioColocado=true; J.rx=J.x; J.ry=J.y;
                        }
                    }else if(J.relogioColocado){
                        detonarRelogioSeArmado(E,J);
                    }
                }

                if(dentro(nx,ny)){
                    Bloco b = E.mapa[indice(nx,ny)];
                    if(b==Bloco::Vazio || (b==Bloco::Quebravel && J.podeAtravessar)){
                        if(nx!=J.x || ny!=J.y) E.est.movimentos++;
                        J.x=nx; J.y=ny; coletarItem(J,E);
                    }
                }
            }

            // J2
            if(E.jogadores.size()>=2 && E.jogadores[1].vidas>0){
                auto& J=E.jogadores[1];
                int nx=J.x, ny=J.y;
                if(t==TECLA_CIMA) nx--;
                else if(t==TECLA_BAIXO) nx++;
                else if(t==TECLA_ESQ) ny--;
                else if(t==TECLA_DIR) ny++;
                else if(t=='n'||t=='N'){
                    plantarBomba(E,J.x,J.y,2,J);
                }else if(t=='m'||t=='M'){
                    if(!J.relogioColocado && J.relogioCargas>0){
                        if(!E.tipoBomba[J.x][J.y]){
                            plantarBomba(E,J.x,J.y,2,J,true);
                            J.relogioColocado=true; J.rx=J.x; J.ry=J.y;
                        }
                    }else if(J.relogioColocado){
                        detonarRelogioSeArmado(E,J);
                    }
                }
                if(dentro(nx,ny)){
                    Bloco b = E.mapa[indice(nx,ny)];
                    if(b==Bloco::Vazio || (b==Bloco::Quebravel && J.podeAtravessar)){
                        if(nx!=J.x || ny!=J.y) E.est.movimentos++;
                        J.x=nx; J.y=ny; coletarItem(J,E);
                    }
                }
            }
        }
        // -------------------------------------------------------------------------------------------

        // Sistemas
        atualizarBombas(E);
        atualizarExplosoes(E);
        moverInimigos(E);
        moverChefe(E);

        // Mortes por explosão
        for(auto& m:E.inimigos){
            if(m.vivo && E.mascaraExpl[m.x][m.y]){
                m.vivo=false; E.est.inimigosMortos++; E.est.pontos+=PTS_MATAR_INIM;
            }
        }
        if(E.chefe.vivo && E.mascaraExpl[E.chefe.x][E.chefe.y]){
            int onda = E.ondaCel[E.chefe.x][E.chefe.y];
            if(onda>0 && E.chefe.ultimaOndaAtingida != onda){
                E.chefe.ultimaOndaAtingida = onda;   // 1 hit por bomba
                if(--E.chefe.hp<=0){ E.chefe.vivo=false; E.est.pontos+=PTS_MATAR_CHEFE; E.est.chefeMorto=1; }
            }
        }

        // Dano nos jogadores
        auto aplicarDanoJogador=[&](Jogador& J, bool j1){
            bool expl = (J.vidas>0 && E.mascaraExpl[J.x][J.y]);
            bool contato=false;
            if(!contato){
                for(const auto& m:E.inimigos) if(m.vivo && m.x==J.x && m.y==J.y){ contato=true; break; }
            }
            if(E.chefe.vivo && J.x==E.chefe.x && J.y==E.chefe.y) contato=true;

            if(expl || contato){
                int res = aplicarDano(J, expl && !contato);
                if(res==2){
                    J.x = -1;
                    J.y = -1;
                    J.relogioColocado = false;
                    J.rx = J.ry = -1;
                }
                else if(res==1){
                    if(j1){ J.x=1; J.y=1; }
                    else  { J.x=1; J.y=COLUNAS-2; }
                }
            }
        };

        if(E.jogadores.size()>=1 && E.jogadores[0].vidas>0) aplicarDanoJogador(E.jogadores[0],true);
        if(E.jogadores.size()>=2 && E.jogadores[1].vidas>0) aplicarDanoJogador(E.jogadores[1],false);

        // derrota?
        bool todosKo=true;
        for(const auto& J:E.jogadores) if(J.vidas>0) { todosKo=false; break; }
        if(todosKo){
            E.est.tempoMs = agoraMilissegundos()-inicio;
            return false;
        }

        // portal
        bool inimVivos=false; for(const auto& m:E.inimigos) if(m.vivo){ inimVivos=true; break; }
        if(!inimVivos && !E.chefe.vivo && !E.portal.aberto) E.portal = abrirPortalEmVazio(E);

        // passagem de fase
        bool algumNoPortal=false;
        if(E.portal.aberto){
            for(const auto& J:E.jogadores){
                if(J.vidas>0 && J.x==E.portal.x && J.y==E.portal.y){ algumNoPortal=true; break; }
            }
        }
        if(algumNoPortal){
            E.est.tempoMs = agoraMilissegundos()-inicio;
            return true;
        }

        Sleep(25);
    }
}


// Ranking / Interfaces

vector<RegistroRanking> lerRanking(){
    vector<RegistroRanking> v; ifstream fin(ARQ_RANK); if(!fin.good()) return v;
    string linha; getline(fin,linha);
    while(getline(fin,linha)){
        if(linha.empty()) continue;
        stringstream ss(linha); RegistroRanking e; string t;

        getline(ss,e.data,',');        e.data        = aparar(e.data);
        getline(ss,e.jogadores,',');   e.jogadores   = aparar(e.jogadores);
        getline(ss,e.dificuldade,','); e.dificuldade = aparar(e.dificuldade);

        getline(ss,t,','); e.pontos     = stod(aparar(t));
        getline(ss,t,','); e.fases      = stoi(aparar(t));
        getline(ss,t,','); e.venceu     = stoi(aparar(t));
        getline(ss,t,','); e.bombas     = stoi(aparar(t));
        getline(ss,t,','); e.movimentos = stoi(aparar(t));
        getline(ss,t,','); e.inimigos   = stoi(aparar(t));
        getline(ss,t,','); e.caixas     = stoi(aparar(t));
        getline(ss,t,','); e.itens      = stoi(aparar(t));
        getline(ss,t,','); e.tempoSeg   = stoi(aparar(t));

        v.push_back(e);
    }
    fin.close();

    // Bubble sort simples
    for (size_t i = 0; i < v.size(); ++i) {
        for (size_t j = 1; j < v.size() - i; ++j) {
            bool trocaFlag = false;
            if (v[j-1].pontos < v[j].pontos) trocaFlag = true;
            else if (v[j-1].pontos == v[j].pontos && v[j-1].tempoSeg > v[j].tempoSeg) trocaFlag = true;
            if (trocaFlag) {
                auto tmp=v[j-1]; v[j-1]=v[j]; v[j]=tmp;
            }
        }
    }
    return v;
}
void mostrarRanking(){
    limparTela(); definirCor(COR_FORTE);
    cout<<"====================== R A N K I N G ======================\n\n"; definirCor(COR_PADRAO);
    auto v=lerRanking();
    if(v.empty()){ definirCor(COR_CINZA); cout<<"Sem registros ainda.\n\n"; definirCor(COR_PADRAO); pausar(); return; }
    definirCor(COR_CIANO);
    cout<<left<<setw(5)<<"#"<<setw(20)<<"Data"<<setw(22)<<"Jogadores"<<setw(9)<<"Dif."
        <<setw(10)<<"Pontos"<<setw(7)<<"Fases"<<setw(7)<<"Win"<<setw(8)<<"Bombas"
        <<setw(10)<<"Movim."<<setw(10)<<"Inims."<<setw(8)<<"Caixas"<<setw(7)<<"Itens"<<setw(8)<<"Tempo"<<"\n";
    definirCor(COR_PADRAO);
    int lim = (v.size() < 20 ? (int)v.size() : 20);
    for(int i=0;i<lim;i++){
        auto &e=v[i];
        definirCor(i==0?COR_AMARELO:COR_PADRAO);
        cout<<left<<setw(5)<<(i+1); definirCor(COR_PADRAO);
        cout<<left<<setw(20)<<e.data<<setw(22)<<e.jogadores.substr(0,21)<<setw(9)<<e.dificuldade;
        definirCor(COR_FORTE); cout<<setw(10)<<fixed<<setprecision(2)<<e.pontos; definirCor(COR_PADRAO);
        cout<<setw(7)<<e.fases<<setw(7)<<(e.venceu?"Sim":"Nao")<<setw(8)<<e.bombas<<setw(10)<<e.movimentos
            <<setw(10)<<e.inimigos<<setw(8)<<e.caixas<<setw(7)<<e.itens<<setw(8)<<e.tempoSeg<<"\n";
    }
    cout<<"\n"; pausar();
}
void mostrarRegras(){
    limparTela(); definirCor(COR_FORTE);
    cout<<"================= R E G R A S  D E  P O N T U A C A O =================\n\n"; definirCor(COR_PADRAO);
    cout<<"Ganha:\n"; definirCor(COR_VERDE);
    cout<<"  + "<<PTS_ITEM<<" por ITEM coletado\n";
    cout<<"  + "<<PTS_CAIXA<<" por CAIXA destruida\n";
    cout<<"  + "<<PTS_MATAR_INIM<<" por INIMIGO abatido\n";
    cout<<"  + "<<PTS_MATAR_CHEFE<<" por CHEFE abatido (na Fase 3)\n";
    cout<<"  + "<<BONUS_VITORIA<<" de BONUS por vencer a campanha\n\n";
    definirCor(COR_PADRAO);
    cout<<"Penaliza no final:\n"; definirCor(COR_VERMELHO);
    cout<<"  - "<<PEN_MOV<<" por MOVIMENTO\n";
    cout<<"  - "<<PEN_BOMBA<<" por BOMBA usada (normal ou relogio)\n";
    cout<<"  - "<<PEN_CONTINUAR<<" por CONTINUAR da fase (cada uso)\n\n"; definirCor(COR_PADRAO);
    cout<<"Formula:\n"; definirCor(COR_CIANO);
    cout<<"  Final = (Eventos + BonusVitoria) - (Mov*"<<PEN_MOV<<") - (Bomb*"<<PEN_BOMBA<<") - (Continuar*"<<PEN_CONTINUAR<<")\n\n";
    definirCor(COR_PADRAO);
    cout<<"Ranking ordena por Pontuacao Final (desempate: menor tempo total).\n\n";
    pausar();
}
void mostrarInstrucoes(){
    limparTela(); definirCor(COR_FORTE);
    cout<<"========== INSTRUCOES ==========\n\n"; definirCor(COR_PADRAO);
    cout<<"Solo: WASD/B/R (ou setas para mover)\n";
    cout<<"Coop: J1 (WASD/B/R)  |  J2 (Setas/N/M)\n\n";
    cout<<"Relogio: pressiona para colocar (se tiver carga) e novamente para detonar (consome 1).\n";
    cout<<"Itens (70% ao quebrar caixa): Fogo+, Bombas+, Vida+, Relogio, Imunidade, Atravessar.\n";
    cout<<"Imunidade: protege contra explosoes durante toda a fase.\n";
    cout<<"Vidas: Facil=3, Medio=2, Dificil=1.\n";
    cout<<"Portal aparece apos matar todos os inimigos. Entre no 'O'.\n";
    cout<<"Fase 3 tem CHEFE (X) que precisa de 3 bombas.\n\n";
    pausar();
}

int menuPrincipal(){
    int op;
    while(true){
        limparTela(); definirCor(COR_FORTE);
        cout<<"=====================================\n";
        cout<<"        B O M B E R M A N\n";
        cout<<"=====================================\n\n"; definirCor(COR_PADRAO);
        cout<<"1. Jogar (1 Jogador - Solo)\n";
        cout<<"2. Jogar (2 Jogadores - Coop)\n";
        cout<<"3. Instrucoes\n";
        cout<<"4. Regras de Pontuacao\n";
        cout<<"5. Ranking\n";
        cout<<"6. Sair\n\n";
        cout<<"Escolha: "; cin>>op;
        if(op>=1&&op<=6) return op;
        cin.clear(); cin.ignore(10000,'\n');
    }
}
int menuDificuldade(){
    int op;
    while(true){
        limparTela(); definirCor(COR_FORTE);
        cout<<"======== ESCOLHA A DIFICULDADE ========\n\n"; definirCor(COR_PADRAO);
        cout<<"1. Facil (3 inimigos)\n";
        cout<<"2. Medio (5 inimigos)\n";
        cout<<"3. Dificil (7 inimigos)\n\n";
        cout<<"Escolha: "; cin>>op;
        if(op>=1&&op<=3) return op;
        cin.clear(); cin.ignore(10000,'\n');
    }
}
int menuPosJogo(bool vitoria, bool podeContinuar){
    int op;
    while(true){
        limparTela(); definirCor(COR_FORTE);
        cout<<"=============================\n";
        if(vitoria){ definirCor(COR_VERDE);  cout<<"   PARABENS! CAMPANHA VENCIDA!\n"; }
        else        { definirCor(COR_VERMELHO); cout<<"           FIM DE JOGO!\n"; }
        definirCor(COR_FORTE); cout<<"=============================\n\n"; definirCor(COR_PADRAO);

        if(!vitoria && podeContinuar){
            cout<<"1. Continuar da fase ( -"<<PEN_CONTINUAR<<" pts )\n";
            cout<<"2. Jogar novamente (do inicio)\n";
            cout<<"3. Voltar ao menu\n";
            cout<<"4. Sair\n\nEscolha: ";
            cin>>op; if(op>=1&&op<=4) return op;
        }else{
            cout<<"1. Jogar novamente\n";
            cout<<"2. Voltar ao menu\n";
            cout<<"3. Sair\n\nEscolha: ";
            cin>>op; if(op>=1&&op<=3) return op;
        }
        cin.clear(); cin.ignore(10000,'\n');
    }
}


// Campanhas

void salvarCSV(const string& jogadores, const ConfiguracaoDificuldade& cfg, const EstatisticasFase& total,
               double penaContinuar, int fasesJogadas, bool venceu, int itensTotalCampanha){
    double baseEventos = total.pontos;
    double bonus       = venceu ? BONUS_VITORIA : 0.0;
    double pen         = total.movimentos*PEN_MOV + total.bombasUsadas*PEN_BOMBA + penaContinuar;
    double finalPts    = max(0.0, baseEventos + bonus - pen);

    ifstream fin(ARQ_RANK); bool existe=fin.good(); fin.close();
    ofstream fout(ARQ_RANK, ios::app);
    if(!existe){
        fout<<"Data, Jogadores, Dificuldade, Pontuacao, Fases, Venceu, Bombas, Movimentos, Inimigos, Caixas, Itens, TempoSeg\n";
    }
    string data=dataHoraAtual(); int tempoSeg=(int)(total.tempoMs/1000);
    fout<<sanitizarCSV(data)<<", "<<sanitizarCSV(jogadores)<<", "<<cfg.nome<<", "
        <<fixed<<setprecision(2)<<finalPts<<", "<<fasesJogadas<<", "<<(venceu?1:0)<<", "
        <<total.bombasUsadas<<", "<<total.movimentos<<", "<<(total.inimigosMortos+total.chefeMorto)<<", "
        <<total.caixasQuebradas<<", "<<itensTotalCampanha<<", "<<tempoSeg<<"\n";
    fout.close();

    limparTela(); definirCor(COR_FORTE); cout<<"Resumo da Partida ("<<jogadores<<")\n"; definirCor(COR_PADRAO);
    cout<<"Dificuldade: "<<cfg.nome<<" | Fases "<<(venceu?NUM_FASES:fasesJogadas)<<"/"<<NUM_FASES<<"\n";
    cout<<"Pontos de eventos: "; definirCor(COR_VERDE); cout<<fixed<<setprecision(2)<<baseEventos<<"\n"; definirCor(COR_PADRAO);
    if(bonus>0){ cout<<"Bonus vitoria: "; definirCor(COR_AMARELO); cout<<fixed<<setprecision(2)<<bonus<<"\n"; definirCor(COR_PADRAO); }
    cout<<"Penalidades (mov/bomb/continuar): "; definirCor(COR_VERMELHO); cout<<pen<<"\n"; definirCor(COR_PADRAO);
    cout<<"Itens totais: "; definirCor(COR_CIANO); cout<<itensTotalCampanha<<"\n"; definirCor(COR_PADRAO);
    cout<<"Pontuacao FINAL: "; definirCor(COR_AMARELO); cout<<finalPts<<"\n\n"; definirCor(COR_PADRAO);
    pausar();
}

void campanha(const ConfiguracaoDificuldade& cfg, int nJogadores){
    cin.ignore(10000, '\n');
    string nome1="Jogador", nome2="Jogador2";
    if(nJogadores==1){ definirCor(COR_FORTE); cout<<"Nome do Jogador: "; definirCor(COR_PADRAO); string t; getline(cin,t); if(!t.empty()) nome1=t; }
    else{
        definirCor(COR_FORTE); cout<<"Nome do Jogador 1: "; definirCor(COR_PADRAO); string t; getline(cin,t); if(!t.empty()) nome1=t;
        definirCor(COR_FORTE); cout<<"Nome do Jogador 2: "; definirCor(COR_PADRAO); t.clear(); getline(cin,t); if(!t.empty()) nome2=t;
    }
    string nomes = (nJogadores==1? nome1 : (nome1+" & "+nome2));

    int faseInicial = 1;
    double penaContinuarAcum = 0.0;
    EstatisticasFase total; total.reset();
    int itensTotalCampanha = 0;

    while(true){
        bool venceu = true;
        int fasesConcluidas = 0;
        int faseOndeParou = faseInicial;

        for(int f=faseInicial; f<=NUM_FASES; ++f){
            faseOndeParou = f;
            EstadoJogo E{};
            E.fase=f; E.cfg=cfg;

            E.jogadores.clear();
            E.jogadores.resize(nJogadores);
            for(auto& J:E.jogadores){
                J.vidas=vidasIniciais(cfg);
                J.maxBombas=1; J.bombasAtivas=0; J.raioFogo=1; J.podeAtravessar=false; J.imuneExplosao=false;
                J.relogioCargas=0; J.relogioColocado=false; J.rx=J.ry=-1;
                for(int i=0;i<8;i++) J.itensPegos[i]=0;
            }

            bool ok = jogarFase(E);
            total.somar(E.est);

            for(const auto& J:E.jogadores)
                for(int k=1;k<=6;k++) itensTotalCampanha += J.itensPegos[k];

            if(!ok){ venceu=false; break; }
            fasesConcluidas++;

            if(f<NUM_FASES){
                limparTela(); definirCor(COR_FORTE); cout<<"Fase "<<f<<" concluida!\n";
                definirCor(COR_PADRAO); Sleep(900);
            }
        }

        if(venceu){
            salvarCSV(nomes,cfg,total,penaContinuarAcum,NUM_FASES,true,itensTotalCampanha);
            int pos = menuPosJogo(true,false);
            if(pos==1){ faseInicial = 1; total.reset(); penaContinuarAcum = 0.0; itensTotalCampanha=0; continue; }
            else if(pos==2){ return; }
            else{ exit(0); }
        }

        int pos = menuPosJogo(false,true);
        if(pos==1){ // CONTINUAR
            penaContinuarAcum += PEN_CONTINUAR;
            faseInicial = faseOndeParou;
            continue;
        }else if(pos==2){ // REINICIAR
            faseInicial = 1; total.reset(); penaContinuarAcum = 0.0; itensTotalCampanha=0; continue;
        }else{
            salvarCSV(nomes,cfg,total,penaContinuarAcum,fasesConcluidas,false,itensTotalCampanha);
            if(pos==3) return; else exit(0);
        }
    }
}


// Main

int main(){
    srand((unsigned)time(NULL));
    ocultarCursor();

    PlaySound(TEXT("musica.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

    ConfiguracaoDificuldade facil  = {3, 500, 800,  6, "Facil"};
    ConfiguracaoDificuldade medio  = {5, 400, 600,  8, "Medio"};
    ConfiguracaoDificuldade dificil= {7, 300, 500, 10, "Dificil"};

    while(true){
        int op = menuPrincipal();
        if(op==1 || op==2){
            int d = menuDificuldade();
            ConfiguracaoDificuldade cfg = (d==1?facil:(d==2?medio:dificil));
            if(op==1) campanha(cfg,1);
            else      campanha(cfg,2);
        }else if(op==3){
            mostrarInstrucoes();
        }else if(op==4){
            mostrarRegras();
        }else if(op==5){
            mostrarRanking();
        }else{
            limparTela(); definirCor(COR_CINZA); cout<<"Saindo... Ate a proxima!\n"; definirCor(COR_PADRAO); Sleep(800);
            PlaySound(NULL, NULL, 0); // parar música
            break;
        }
    }
    return 0;
}
