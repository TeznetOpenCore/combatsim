#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cmath>
#include <termios.h>
#include <cstdio>
#include <algorithm>

// ===================== Tiles =====================
const std::string EMPTY="--";
const std::string WALL ="##";
const std::string ENEMY="xD";
const std::string ELITE=";)";
const std::string RX   ="RX";
const std::string MINE ="@@";

// ===================== Colors =====================
const std::string RESET="\033[0m";
const std::string RED="\033[31m";
const std::string GREEN="\033[32m";
const std::string YELLOW="\033[33m";
const std::string BLUE="\033[34m";
const std::string MAGENTA="\033[35m";
const std::string CYAN="\033[36m";
const std::string WHITE="\033[37m";
const std::string PINK="\033[38;2;255;105;180m";

std::vector<std::string> COLORS={RED,GREEN,YELLOW,BLUE,MAGENTA,CYAN,WHITE};
std::string colorA1=WHITE;
std::string colorRX=CYAN;

std::string randomColor(){ return COLORS[rand()%COLORS.size()]; }

// ===================== Globals =====================
std::string currentDialogue="//WASD//1=PUNCH ;) 2=M1SS1LES xD //3=R0B0S0AR UPG.";

bool missileActive=false;
bool rxAbilityUnlocked=false;
bool rxConsumed=false;
bool binaryBeamActive=false;

int xdKills=0;
int eliteMoveTick=0;
int rxMoveTick=0;
int mineSpawnTick=0;

int lastDX=0,lastDY=-1;

std::vector<std::pair<int,int>> missiles;
std::vector<std::pair<int,int>> mines;

struct BeamSeg{int x,y;std::string prev;};
std::vector<BeamSeg> binaryBeam;

// ===================== Helpers =====================
int sign(int v){ return (v>0)-(v<0); }

std::string missileGlyph(int mx,int my,int tx,int ty){
    int dx=sign(tx-mx), dy=sign(ty-my);
    if(abs(dx)>=abs(dy)) return dx>0?">>":"<<";
    return dy>0?"vv":"^^";
}

// ===================== Menu =====================
void showMainMenu(int &GS,int &EC,int &EL){
    std::cout<<"\033[2J\033[H";
    std::cout <<
"========================================\n"
"    AI.F1NALBOSS // TERMINAL COMBAT SIM\n"
"========================================\n\n"
"> Press Enter for Default...\n"
"> Ehh theyll figure it out - N33R\n"
"> X_x\n\n";

    std::string in;
    std::cout<<"xD enemies [23]: ";
    std::getline(std::cin,in);
    EC=in.empty()?23:std::stoi(in);

    std::cout<<"Grid size [23]: ";
    std::getline(std::cin,in);
    GS=in.empty()?23:std::stoi(in);

    std::cout<<";) elites [1]: ";
    std::getline(std::cin,in);
    EL=in.empty()?1:std::stoi(in);

    std::cout<<"\n> PRESS ENTER TO DEPLOY A1\n";
    std::getline(std::cin,in);
}

// ===================== Screen =====================
void resetCursor(){ std::cout<<"\033[H"; }

void printGrid(const std::vector<std::vector<std::string>>& g){
    int GS=g.size();
    for(int y=0;y<GS;y++){
        for(int x=0;x<GS;x++){
            std::string c=g[y][x];
            if(c=="A1") c=colorA1+c+RESET;
            else if(c==RX) c=colorRX+c+RESET;
            std::cout<<"["<<c<<"]";
        }
        std::cout<<"\n";
    }
    std::cout<<"> "<<currentDialogue<<"\n";
}

// ===================== Character =====================
struct Character{
    std::string name;
    int x,y;
    bool invincible;
    Character(std::string n):name(n),x(0),y(0),invincible(true){}
};

// ===================== Input =====================
char getKeyPressNonBlocking(){
    termios o,n; char c=0;
    tcgetattr(STDIN_FILENO,&o);
    n=o; n.c_lflag&=~(ICANON|ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&n);
    fd_set s; timeval t{0,100000};
    FD_ZERO(&s); FD_SET(STDIN_FILENO,&s);
    if(select(STDIN_FILENO+1,&s,nullptr,nullptr,&t)>0) c=getchar();
    tcsetattr(STDIN_FILENO,TCSANOW,&o);
    return c;
}

// ===================== Random Spawn =====================
void randomSpawn(int &x,int &y,std::vector<std::vector<std::string>>& g,Character* p=nullptr){
    int GS=g.size();
    do{
        x=rand()%GS; y=rand()%GS;
        if(p&&abs(x-p->x)<=1&&abs(y-p->y)<=1) continue;
    }while(g[y][x]!=EMPTY);
}

// ===================== Player Input =====================
void handlePlayerInput(
    Character& p,
    std::vector<std::vector<std::string>>& g,
    std::vector<std::pair<int,int>>& elites,
    std::vector<std::pair<int,int>>& enemies,
    char ch
){
    int GS=g.size();

    if(ch=='1'){
        for(auto it=elites.begin();it!=elites.end();){
            if(abs(it->first-p.x)+abs(it->second-p.y)==1){
                if(xdKills>=13){
                    g[it->second][it->first]=EMPTY;
                    it=elites.erase(it);
                }else{
                    currentDialogue="13";
                    printGrid(g);
                    exit(0);
                }
            }else ++it;
        }
    }

    if(ch=='2'){
        missileActive=true;
        missiles.clear();
        for(int dy=-1;dy<=1;dy++)
            for(int dx=-1;dx<=1;dx++){
                if(!dx&&!dy) continue;
                int x=p.x+dx,y=p.y+dy;
                if(x>=0&&y>=0&&x<GS&&y<GS&&g[y][x]==EMPTY)
                    missiles.push_back({x,y});
            }
    }

    if(ch=='3'&&rxAbilityUnlocked&&!binaryBeamActive){
        binaryBeamActive=true;
        binaryBeam.clear();
        int bx=p.x,by=p.y;
        std::string glyph=(abs(lastDX)==1)?"==":"||";
        for(int i=0;i<GS;i++){
            bx+=lastDX; by+=lastDY;
            if(bx<0||by<0||bx>=GS||by>=GS) break;
            if(g[by][bx]==ELITE) break;
            auto it=std::find(enemies.begin(),enemies.end(),std::make_pair(bx,by));
            if(it!=enemies.end()){
                enemies.erase(it);
                xdKills++;
                g[by][bx]=EMPTY;
            }
            binaryBeam.push_back({bx,by,g[by][bx]});
            g[by][bx]=CYAN+glyph+RESET;
        }
    }

    int dx=(ch=='d')-(ch=='a');
    int dy=(ch=='s')-(ch=='w');
    if(!dx&&!dy) return;

    int nx=p.x+dx, ny=p.y+dy;
    if(nx<0||ny<0||nx>=GS||ny>=GS) return;

    if(g[ny][nx]==RX&&!rxConsumed){
        rxConsumed=true;
        rxAbilityUnlocked=true;
        g[ny][nx]=EMPTY;
    }else if(g[ny][nx]!=EMPTY&&g[ny][nx]!=MINE) return;

    if(g[ny][nx]==MINE){
        currentDialogue="M1NE H3H3";
        printGrid(g);
        exit(0);
    }

    g[p.y][p.x]=EMPTY;
    p.x=nx; p.y=ny;
    g[p.y][p.x]=p.name;
    lastDX=dx; lastDY=dy;
    p.invincible=false;
}

// ===================== Missiles =====================
std::pair<int,int> nearestEnemy(int mx,int my,std::vector<std::pair<int,int>>& e){
    int best=9999; std::pair<int,int> t{mx,my};
    for(auto&a:e){
        int d=abs(a.first-mx)+abs(a.second-my);
        if(d<best){best=d;t=a;}
    }
    return t;
}

void updateMissiles(std::vector<std::vector<std::string>>& g,std::vector<std::pair<int,int>>& enemies){
    if(!missileActive) return;
    std::vector<std::pair<int,int>> next;
    for(auto&m:missiles){
        g[m.second][m.first]=EMPTY;
        auto t=nearestEnemy(m.first,m.second,enemies);
        int cx=m.first+sign(t.first-m.first);
        int cy=m.second+sign(t.second-m.second);

        if(g[cy][cx]==ENEMY){
            g[cy][cx]=EMPTY;
            enemies.erase(std::remove(enemies.begin(),enemies.end(),std::make_pair(cx,cy)),enemies.end());
            xdKills++;
        }else if(g[cy][cx]==EMPTY){
            g[cy][cx]=PINK+missileGlyph(cx,cy,t.first,t.second)+RESET;
            next.push_back({cx,cy});
        }
    }
    missiles=next;
}

// ===================== Binary Beam =====================
void updateBinaryBeam(std::vector<std::vector<std::string>>& g){
    if(!binaryBeamActive) return;
    static int life=5;
    if(--life<=0){
        for(auto&b:binaryBeam)
            g[b.y][b.x]=b.prev;
        binaryBeam.clear();
        binaryBeamActive=false;
        life=5;
    }
}

// ===================== Enemies =====================
void updateEnemies(Character&p,std::vector<std::vector<std::string>>& g,std::vector<std::pair<int,int>>& enemies){
    for(auto&e:enemies){
        int dx=p.x-e.first, dy=p.y-e.second;
        if(abs(dx)+abs(dy)<=4){
            int nx=e.first+sign(dx);
            int ny=e.second+sign(dy);
            if(nx==p.x&&ny==p.y&&!p.invincible){
                currentDialogue="TERMINATED!";
                printGrid(g);
                exit(0);
            }
            if(g[ny][nx]==EMPTY){
                g[e.second][e.first]=EMPTY;
                e={nx,ny};
                g[ny][nx]=ENEMY;
            }
        }
    }
}

// ===================== Elites =====================
void updateElites(Character&p,std::vector<std::vector<std::string>>& g,std::vector<std::pair<int,int>>& elites){
    eliteMoveTick++;
    if(eliteMoveTick>=33){
        eliteMoveTick=0;
        for(auto&e:elites){
            int nx=e.first+sign(p.x-e.first);
            int ny=e.second+sign(p.y-e.second);
            if(g[ny][nx]==EMPTY){
                g[e.second][e.first]=EMPTY;
                e={nx,ny};
                g[ny][nx]=ELITE;
            }
        }
    }

    mineSpawnTick++;
    if(mineSpawnTick>=100){
        mineSpawnTick=0;
        for(auto&e:elites){
            std::vector<std::pair<int,int>> free;
            for(int dy=-1;dy<=1;dy++)
                for(int dx=-1;dx<=1;dx++){
                    if(!dx&&!dy) continue;
                    int mx=e.first+dx,my=e.second+dy;
                    if(mx>=0&&my>=0&&mx<g.size()&&my<g.size()
                       &&g[my][mx]==EMPTY
                       &&std::find(mines.begin(),mines.end(),std::make_pair(mx,my))==mines.end())
                        free.push_back({mx,my});
                }
            if(!free.empty()){
                auto c=free[rand()%free.size()];
                g[c.second][c.first]=MINE;
                mines.push_back(c);
            }
        }
    }
}

// ===================== RX =====================
void updateRX(std::vector<std::vector<std::string>>& g,std::pair<int,int>& rxPos){
    if(rxConsumed) return;
    rxMoveTick++;
    if(rxMoveTick<20) return;
    rxMoveTick=0;
    int nx=rxPos.first+(rand()%3-1);
    int ny=rxPos.second+(rand()%3-1);
    if(nx<0||ny<0||nx>=g.size()||ny>=g.size()) return;
    if(g[ny][nx]==EMPTY){
        g[rxPos.second][rxPos.first]=EMPTY;
        rxPos={nx,ny};
        g[ny][nx]=RX;
    }
}

// ===================== Victory =====================
void printVictory() {
    std::cout << "\033[2J\033[H";
    std::cout <<
"██╗   ██╗██╗ ██████╗████████╗ ██████╗ ██████╗ ██╗   ██╗\n"
"██║   ██║██║██╔════╝╚══██╔══╝██╔═══██╗██╔══██╗╚██╗ ██╔╝\n"
"██║   ██║██║██║        ██║   ██║   ██║██████╔╝ ╚████╔╝ \n"
"╚██╗ ██╔╝██║██║        ██║   ██║   ██║██╔══██╗  ╚██╔╝  \n"
" ╚████╔╝ ██║╚██████╗   ██║   ╚██████╔╝██║  ██║   ██║   \n"
"  ╚═══╝  ╚═╝ ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝   ╚═╝   \n";
    std::cout << "\n\n";
}


// ===================== MAIN =====================
int main(){
    srand(time(0));

    int GS,EC,EL;
    showMainMenu(GS,EC,EL);

    std::vector<std::vector<std::string>> grid(GS,std::vector<std::string>(GS,EMPTY));
    for(int i=0;i<GS*4;i++) grid[rand()%GS][rand()%GS]=WALL;

    Character A1("A1");
    randomSpawn(A1.x,A1.y,grid);
    grid[A1.y][A1.x]=A1.name;

    std::vector<std::pair<int,int>> enemies,elites;
    for(int i=0;i<EC;i++){
        int x,y; randomSpawn(x,y,grid,&A1);
        grid[y][x]=ENEMY; enemies.push_back({x,y});
    }
    for(int i=0;i<EL;i++){
        int x,y; randomSpawn(x,y,grid,&A1);
        grid[y][x]=ELITE; elites.push_back({x,y});
    }

    std::pair<int,int> rxPos;
    randomSpawn(rxPos.first,rxPos.second,grid,&A1);
    grid[rxPos.second][rxPos.first]=RX;

    std::cout<<"\033[2J";

    while(true){
        colorA1=randomColor();
        colorRX=randomColor();

        char ch=getKeyPressNonBlocking();
        handlePlayerInput(A1,grid,elites,enemies,ch);
        updateMissiles(grid,enemies);
        updateBinaryBeam(grid);
        updateEnemies(A1,grid,enemies);
        updateElites(A1,grid,elites);
        updateRX(grid,rxPos);

        if(enemies.empty()&&elites.empty()){
            printVictory();
            return 0;
        }

        resetCursor();
        printGrid(grid);
        usleep(40000);
    }
}

