#include <sys/time.h>
#include <sys/types.h>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

using namespace std;

typedef int32_t i32; //in32_t is to be exactly 32 bit long.It is used to manage space with care
typedef int64_t i64;
int a;
int cas;

struct graph {
  struct ds_t {
    i32 val;
    vector<pair<i32, i32>> st;
    ds_t() : val(), st() {}
  };
  // input parameters
  i32 theta, k, tau;
  // input data
  i32 V, E;
  vector<vector<pair<pair<i32, i32>, pair<i32, i32>>>> neighbors;
  // output data
  vector<ds_t> ds;  //used to store meta-intervals and degree persistence in TGR
  vector<vector<i32>> ans;
  // helper data
  vector<bool> visited, is_selected, &in_comp;
  vector<i32> par;
  
  timeval start_at, end_at;
  graph(istream& in, i32 theta, i32 k, i32 tau)
      : theta(theta),
        k(k),
        tau(tau),
        V(),
        E(),
        neighbors(),
        ans(),
        visited(),
        is_selected(),
        in_comp(visited),
        start_at(),
        end_at() {
    gettimeofday(&start_at, 0);
    assert(theta <= tau);
    in >> V >> E;
    neighbors.resize(V);
    for (i32 i = 0, last_t = 0; i < E; ++i) {  // for each edge(u,v,t) adjacent on u,v at timestamp t
      i32 u, v, t;
      in >> u >> v >> t;
      assert(0 <= u && u < V);  // Nodes of graph should be from 0 (inclusive) to V (exclusive)
      assert(0 <= v && v < V);
      assert(u != v);
      assert(last_t <= t);  // timestamp edges are in increasing order
      i32 bu = neighbors[u].size(), bv = neighbors[v].size();
      neighbors[u].emplace_back(make_pair(v, bv), make_pair(t, t + theta));  //v is neighbor of u
      neighbors[v].emplace_back(make_pair(u, bu), make_pair(t, t + theta));   //and u is neighbor of v
      last_t = t;
    }
    gettimeofday(&end_at, 0);
    cout << "reading time: "
         << (end_at.tv_sec - start_at.tv_sec) * 1000 +
                (end_at.tv_usec - start_at.tv_usec) / 1000
         << "ms" << endl;
  }
  // NOTE: O(V+theta E)
  void theta_stable_k_degree_nodes_with_stability_no_less_than_tau() {  //contains meta-interval decomposition
    gettimeofday(&start_at, 0); //pre-processing part of the algo
    
    vector<i32> last(V);
    vector<vector<bool>> ignore(V);
    for (i32 u = 0; u < V; u++) {
      ignore[u].resize(neighbors[u].size());
      for (i32 i = 0; i < (i32)neighbors[u].size(); i++) {
        auto& n = neighbors[u][i];
        i32 j = last[n.first.first];
        if (j < i && neighbors[u][j].first.first == n.first.first) {
          if (neighbors[u][j].second.second > n.second.first) {
            neighbors[u][j].second.second = n.second.first;
            ignore[u][i] = true;
          }
        }
        last[n.first.first] = i;
      }
    }
    queue<i32> q;
    vector<bool> inq(V);
    for (i32 u = 0; u < V; u++) {
      if (neighbors[u].size() == 0) {
        ds[u].val = 0;    //ds[u].val contains the aggregated degree similar lo Du in the paper
      } else {
        
        for (i32 i = 0, j = 0;
             i < (i32)neighbors[u].size() || j < (i32)neighbors[u].size();) {
          if (i < (i32)neighbors[u].size()) {
            if (j < (i32)neighbors[u].size()) {
              if (neighbors[u][i].second.first <
                  neighbors[u][j].second.second) {
                ds[u].st.emplace_back(neighbors[u][i].second.first, i);
                i++;
              } else {
                if (neighbors[u][j].second.second ==
                    neighbors[u][j].second.first + theta) {
                  ds[u].st.emplace_back(neighbors[u][j].second.second, -1);
                }
                j++;
              }
            } else {
              ds[u].st.emplace_back(neighbors[u][i].second.first, i);
              i++;
            }
          } else {
            if (neighbors[u][j].second.second ==
                neighbors[u][j].second.first + theta) {
              ds[u].st.emplace_back(neighbors[u][j].second.second, -1);
            }
            j++;
          }
        }
        i32 ns = 0;
        for (i32 i = 0, pre = 0; i < (i32)ds[u].st.size(); i++) {
          if (ns > 0 && ds[u].st[ns - 1].first == ds[u].st[i].first) {
            if (ds[u].st[i].second == -1) {
              pre--;
            } else {
              neighbors[u][ds[u].st[i].second].second.first = ns - 1;
              if (!ignore[u][ds[u].st[i].second]) {
                pre++;
              }
            }
            ds[u].st[ns - 1].second = pre;
          } else {
            ns++;
            if (ds[u].st[i].second == -1) {
              pre--;
            } else {
              neighbors[u][ds[u].st[i].second].second.first = ns - 1;
              if (!ignore[u][ds[u].st[i].second]) {
                pre++;
              }
            }
            ds[u].st[ns - 1].first = ds[u].st[i].first;
            ds[u].st[ns - 1].second = pre;
          }
        }
        ds[u].st.resize(ns);
        for (i32 i = 0; i < ns - 1; i++) {
          if (ds[u].st[i].second >= k) {
            ds[u].val += ds[u].st[i + 1].first - ds[u].st[i].first;
          }
        }
      }
    
      if (ds[u].val < tau - theta) {
        inq[u] = true;
        q.emplace(u);
        
      }
    }    //until here we pushed the nodes with persistence less than tau into queue q
    while (!q.empty()) {
      i32 u = q.front();
     //we pop each node from queue and delete it along with the edged incident on it and calculate the degree persistence of the neighboring nodes
      for (auto& n : neighbors[u]) {
        i32 v = n.first.first;
        i32 it = neighbors[v][n.first.second].second.first;
        // NOTE: O(theta)
        while (ds[v].st[it].first < n.second.second) {
          ds[v].st[it].second--;
          if (ds[v].st[it].second + 1 >= k && ds[v].st[it].second < k) {
            i32 delta = ds[v].st[it + 1].first - ds[v].st[it].first;
            ds[v].val -= delta;
            if (ds[v].val + delta >= tau - theta && ds[v].val < tau - theta &&
                !inq[v]) {  ////if after deleting the edge if degree persistence of neighbors is less than tau then push them into queue q
              q.emplace(v);
              inq[v] = true;
            }
          }
          it++;
        }
      }
      q.pop();
    } //completion of TGR algorithm
    gettimeofday(&end_at, 0);
    cout << "preprocessing time: "
         << (end_at.tv_sec - start_at.tv_sec) * 1000 +
                (end_at.tv_usec - start_at.tv_usec) / 1000
         << "ms" << endl;
    i64 idxsz = 0;
    for (auto& d : ds) {
      idxsz += 4 + 4 * 2 * d.st.size();
    }
    

  }

  i32 find(i32 x) {
    if (par[x] == x) {
      return x;
    } else {
      par[x] = find(par[x]);
      return par[x];
    }
  }
  pair<i32, i32> stability(const vector<i32>& comp, i32 selected) { //meta-interval intersection algorithm
    vector<pair<i32, i32>> st;
    st.emplace_back(make_pair(0, numeric_limits<i32>::max()));
    i32 len = numeric_limits<i32>::max(),
        selected_len = numeric_limits<i32>::max();
    for (i32 i = 0; i < (i32)comp.size(); i++) {
      i32 u = comp[i];
      vector<pair<i32, i32>> nst;
      i32 nlen = 0;
      auto it = st.begin();
      for (i32 j = 0; j < (i32)ds[u].st.size(); j++) {
        if (ds[u].st[j].second >= k) {
          // [ds[u].st[j].first, ds[u].st[j + 1].first);
          while (it != st.end() && it->second <= ds[u].st[j].first) {
            it++;
          }
          while (it != st.end() && it->second <= ds[u].st[j + 1].first) {
            pair<i32, i32> range(max(it->first, ds[u].st[j].first), it->second);
            if (range.first < range.second) {
              nst.emplace_back(range);
              nlen += range.second - range.first;
            }
            it++;
          }
          if (it != st.end() && it->first < ds[u].st[j + 1].first) {
            pair<i32, i32> range(max(it->first, ds[u].st[j].first),
                                 ds[u].st[j + 1].first);
            if (range.first < range.second) {
              nst.emplace_back(range);
              nlen += range.second - range.first;
            }
          }
        }
      }
      st.swap(nst);
      len = nlen;
      if (i < selected) {
        selected_len = len;
      }
      if (len < tau - theta) {
        break;
      }
      if (i == selected - 1) {
        
        if (len >= tau - theta) {
          for (i32 i = 0; i < (i32)comp.size(); i++) {
            i32 u = comp[i];
            par[u] = u;
          }
          for (i32 u : comp) {
            for (auto& n : neighbors[u]) {
              i32 v = n.first.first;
              if (in_comp[v]) {
                i32 b = ds[u].st[n.second.first].first;
                i32 e = n.second.second;
                
                bool is_active = false;
                i32 it = n.second.first;
                while (ds[u].st[it].first < n.second.second) {
                  if (ds[u].st[it].second >= k) {
                    is_active = true;
                    break;
                  }
                  it++;
                }
                if (is_active) {
                  i32 j =
                      upper_bound(st.begin(), st.end(),
                                  make_pair(e, numeric_limits<i32>::max())) -
                      st.begin() - 1;
                  if (j != -1 && b <= st[j].second) {
                    par[find(u)] = find(v);
                  }
                }
              }
            }
          }
          for (i32 i = 1; i < selected; i++) {
            i32 u = comp[i];
            if (find(comp[0]) != find(u)) {
              return make_pair(-1, -1);
            }
          }
         
        }
      }
    }
    return make_pair(len, selected_len);
  }//completion of meta-interval intersection algo
  bool remove_node(i32 u, queue<i32>& q) {
    bool removable = true;
    for (auto& n : neighbors[u]) {
      i32 v = n.first.first;
      i32 it = neighbors[v][n.first.second].second.first;
      while (ds[v].st[it].first < n.second.second) {
        ds[v].st[it].second--;
        if (ds[v].st[it].second + 1 >= k && ds[v].st[it].second < k) {
          i32 delta = ds[v].st[it + 1].first - ds[v].st[it].first;
          ds[v].val -= delta;   //// update degree persistence
          if (ds[v].val + delta >= tau - theta && ds[v].val < tau - theta) {
            q.emplace(v);
            if (is_selected[v]) {
              removable = false;
            }
          }
        }
        it++;
      }
    }
    return removable;
  }
  void add_node(i32 u) {
    for (auto& n : neighbors[u]) {
      i32 v = n.first.first;
      i32 it = neighbors[v][n.first.second].second.first;
      while (ds[v].st[it].first < n.second.second) {
        ds[v].st[it].second++;
        if (ds[v].st[it].second >= k && ds[v].st[it].second - 1 < k) {
          i32 delta = ds[v].st[it + 1].first - ds[v].st[it].first;
          ds[v].val += delta;
        }
        it++;
      }
    }
  }
  void branch_and_bound(const vector<i32>& comp, i32 selected) {
    static bool exit;
    gettimeofday(&end_at, 0);
    if (end_at.tv_sec - start_at.tv_sec > 1000) {
      exit = true;
    }
    if (exit) {
      return;
    }
  
    if (ans.back().size() >= comp.size()) {
      return;
    }
   
    auto len = selected == 0 ? make_pair(-1, numeric_limits<i32>::max())
                             : stability(comp, selected);
    
    if (len.first >= tau - theta) {
      ans.emplace_back(comp);
    } else if (len.second >= tau - theta && selected < (i32)comp.size()) {
      // NOTE: important
      vector<bool> connected(comp.size(), true);
      if (selected) {
        for (i32 i = 0; i < (i32)comp.size(); i++) {
          connected[i] = find(comp[i]) == find(comp[0]);
        }
      }
      vector<i32> compr, comps;
      i32 u = comp[selected];
      // remove u
      if (!exit) {
        
        bool removable = true;
        vector<i32> removed;
        queue<i32> q;
        vector<pair<i32, i32>> backup;
        for (i32 i = 0; i < (i32)comp.size(); i++) {
          i32 v = comp[i];
          if (v == u || !connected[i]) {
            backup.emplace_back(v, ds[v].val);
            ds[v].val = 0;
            q.emplace(v);
          }
        }
        while (!q.empty()) {
          i32 v = q.front();
          removed.emplace_back(v);
          in_comp[v] = false;
          if (!remove_node(v, q)) {
            removable = false;
            break;
          }
          q.pop();
        }
        if (removable) {
          for (i32 v : comp) {
            if (in_comp[v]) {
              compr.emplace_back(v);
            }
          }
          branch_and_bound(compr, selected);
        }
        for (i32 v : removed) {
          in_comp[v] = true;
          add_node(v);
        }
        for (auto b : backup) {
          ds[b.first].val = b.second;
        }
      }
      // select u
      if (!exit) {
        is_selected[u] = true;
        selected++;
        
        bool selectable = true;
        vector<pair<i32, i32>> st;
        st.emplace_back(make_pair(0, numeric_limits<i32>::max()));
        i32 len = numeric_limits<i32>::max();
        for (i32 i = 0; i < selected; i++) {
          i32 v = comp[i];
          vector<pair<i32, i32>> nst;
          i32 nlen = 0;
          auto it = st.begin();
          for (i32 j = 0; j < (i32)ds[v].st.size(); j++) {
            if (ds[v].st[j].second >= k) {
              // [ds[v].st[j].first, ds[v].st[j + 1].first)
              while (it != st.end() && it->second <= ds[v].st[j].first) {
                it++;
              }
              while (it != st.end() && it->second <= ds[v].st[j + 1].first) {
                pair<i32, i32> range(max(it->first, ds[v].st[j].first),
                                     it->second);
                if (range.first < range.second) {
                  nst.emplace_back(range);
                  nlen += range.second - range.first;
                }
                it++;
              }
              if (it != st.end() && it->first < ds[v].st[j + 1].first) {
                pair<i32, i32> range(max(it->first, ds[v].st[j].first),
                                     ds[v].st[j + 1].first);
                if (range.first < range.second) {
                  nst.emplace_back(range);
                  nlen += range.second - range.first;
                }
              }
            }
          }
          st.swap(nst);
          len = nlen;
          if (len < tau - theta) {
            selectable = false;
            break;
          }
        }
        if (selectable) {
         
          bool removable = true;
          vector<i32> removed;
          queue<i32> q;
          vector<pair<i32, i32>> backup;
          for (i32 i = selected; i < (i32)comp.size(); i++) {
            i32 v = comp[i];
            vector<pair<i32, i32>> nst;
            i32 nlen = 0;
            auto it = st.begin();
            for (i32 j = 0; j < (i32)ds[v].st.size(); j++) {
              if (ds[v].st[j].second >= k) {
                // [ds[v].st[j].first, ds[v].st[j + 1].first)
                while (it != st.end() && it->second <= ds[v].st[j].first) {
                  it++;
                }
                while (it != st.end() && it->second <= ds[v].st[j + 1].first) {
                  pair<i32, i32> range(max(it->first, ds[v].st[j].first),
                                       it->second);
                  if (range.first < range.second) {
                    nst.emplace_back(range);
                    nlen += range.second - range.first;
                  }
                  it++;
                }
                if (it != st.end() && it->first < ds[v].st[j + 1].first) {
                  pair<i32, i32> range(max(it->first, ds[v].st[j].first),
                                       ds[v].st[j + 1].first);
                  if (range.first < range.second) {
                    nst.emplace_back(range);
                    nlen += range.second - range.first;
                  }
                }
              }
            }
            if (nlen < tau - theta || !connected[i]) {
              backup.emplace_back(v, ds[v].val);
              ds[v].val = 0;
              q.emplace(v);
            }
          }
          while (!q.empty()) {
            i32 v = q.front();
            removed.emplace_back(v);
            in_comp[v] = false;
            if (!remove_node(v, q)) {
              removable = false;
              break;
            }
            q.pop();
          }
          if (removable) {
            for (i32 v : comp) {
              if (in_comp[v]) {
                comps.emplace_back(v);
              }
            }
            branch_and_bound(comps, selected);
          }
          for (i32 v : removed) {
            in_comp[v] = true;
            add_node(v);
          }
          for (auto b : backup) {
            ds[b.first].val = b.second;
          }
        }
        selected--;
        is_selected[u] = false;
      }
    }
  }
  void solve() {
    ds.resize(V);
    theta_stable_k_degree_nodes_with_stability_no_less_than_tau();
    visited.resize(V);
    is_selected.resize(V);
    par.resize(V);
    ans.emplace_back();
    gettimeofday(&start_at, 0);
    for (i32 u = 0; u < V; u++) {
      if (ds[u].val >= tau - theta && !visited[u]) {
        vector<i32> comp;
        i32 qh = 0;
        comp.emplace_back(u);
        visited[u] = true;
        while (qh < (i32)comp.size()) {
          i32 u = comp[qh];
          qh++;
          for (auto& n : neighbors[u]) {
            i32 v = n.first.first;
            if (!visited[v] && ds[v].val >= tau - theta) {
              comp.emplace_back(v);
              visited[v] = true;
            }
          }
        }
        random_shuffle(comp.begin(), comp.end());
        branch_and_bound(comp, 0);
      }
    }
    gettimeofday(&end_at, 0);
    cout << "searching time: "
         << (end_at.tv_sec - start_at.tv_sec) * 1000 +
                (end_at.tv_usec - start_at.tv_usec) / 1000
         << "ms" << endl;
         
    if(cas==1){
      cout << "Number of nodes in the final community are " <<ans.back().size()  << endl;
      cout << "Nodes in the final community are " << endl;
      for (i32 u : ans.back()) {
      
      cout << u << endl;
    
      }  
    }
    if(cas==2){
      int fl=0;
      for (i32 u : ans.back()) {
        if(u==a){
          fl=1;
        }
      }
      if(fl==1){
        for (i32 u : ans.back()) {
          if(u!=a){
          cout << u <<endl;
          }
        }
      }
      else
        cout<< "given node is not in the largest community" << endl;
    }
  
  }
};

int main(int argc, char* argv[]) {
   cout<<"enter case";
  cin>>cas;
  if(cas==2){
    cout<<"enter a node ";
  cin>>a;
  }
 
  ifstream in(argv[1]);
  i32 theta = atoi(argv[2]);
  i32 k = atoi(argv[3]);
  i32 tau = atoi(argv[4]);
  graph G(in, theta, k, tau);
  G.solve();

  return 0;
}



