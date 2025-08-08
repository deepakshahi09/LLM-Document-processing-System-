// policy_processor.cpp
// Compile with: g++ -std=c++17 policy_processor.cpp -O2 -o policy_processor
#include <bits/stdc++.h>
#include "json.hpp"
using json = nlohmann::json;
using namespace std;

// helper: lowercase
string lower(string s){ transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

// simple extraction helpers (naive, tailored for demo)
int extract_first_int(const string &s){
    for (size_t i=0;i<s.size();++i){
        if (isdigit((unsigned char)s[i])){
            size_t j=i;
            while (j<s.size() && isdigit((unsigned char)s[j])) ++j;
            return stoi(s.substr(i,j-i));
        }
    }
    return -1;
}

struct ParsedQuery {
    int age = -1;
    string sex;
    string procedure;
    string location;
    int policyMonths = -1;
};

ParsedQuery parse_query(const string &q0){
    string q = lower(q0);
    ParsedQuery p;
    // age: look for 'xx-year' or standalone two-digit
    std::smatch m;
    try {
        std::regex rx_year(R"((\d{2})\s*-\s*year)");
        if (std::regex_search(q, m, rx_year)) p.age = stoi(m[1]);
        else {
            std::regex rx_year2(R"((\d{2})\s*year)");
            if (std::regex_search(q,m,rx_year2)) p.age = stoi(m[1]);
        }
    } catch(...) {}

    if (p.age == -1){
        // fallback: any two-digit number
        p.age = extract_first_int(q);
    }

    if (q.find("male")!=string::npos) p.sex="male";
    else if (q.find("female")!=string::npos) p.sex="female";

    // policy months: "3-month" or "3 month" or "3-month-old" or "3-months"
    try {
        std::regex rx_month(R"((\d+)\s*-?\s*month)");
        if (std::regex_search(q,m,rx_month)) p.policyMonths = stoi(m[1]);
        else {
            // maybe years
            std::regex rx_years(R"((\d+)\s*-?\s*year)");
            if (std::regex_search(q,m,rx_years)) p.policyMonths = stoi(m[1]) * 12;
        }
    } catch(...) {}

    // location: look for 'in {word}'
    try {
        std::regex rx_in(R"(in\s+([A-Za-z\-]+))");
        if (std::regex_search(q,m,rx_in)) p.location = m[1];
    } catch(...) {}

    // procedure: common words 'surgery', phrase around it
    size_t pos = q.find("surgery");
    if (pos!=string::npos){
        size_t start = (pos>=20?pos-20:0);
        string sub = q.substr(start, min((size_t)40, q.size()-start));
        p.procedure = sub;
        // cleanup: try to extract noun before surgery like 'knee'
        size_t k = sub.find("knee");
        if (k!=string::npos) p.procedure = "knee surgery";
        else if (sub.find("cataract")!=string::npos) p.procedure = "cataract surgery";
    } else {
        // fallback keywords
        if (q.find("knee")!=string::npos) p.procedure = "knee surgery";
        if (q.find("cataract")!=string::npos) p.procedure = "cataract surgery";
    }
    return p;
}

// score clause relevance (very simple): count overlap tokens
double score_clause(const string &clause, const ParsedQuery &pq){
    string c = lower(clause);
    double score = 0;
    if (!pq.procedure.empty() && c.find(lower(pq.procedure))!=string::npos) score += 2.0;
    if (!pq.location.empty() && c.find(lower(pq.location))!=string::npos) score += 1.2;
    if (pq.policyMonths>0 && c.find("month")!=string::npos){
        int m = extract_first_int(c);
        if (m>0 && pq.policyMonths >= m) score += 0.8;
    }
    if (c.find("knee")!=string::npos && pq.procedure.find("knee")!=string::npos) score += 1.0;
    return score;
}

// try to extract rupee amount like 1,50,000 INR or 150000 INR or Rs. 1,50,000
int extract_amount(const string &s){
    std::smatch m;
    try {
        std::regex rx(R"((\d{1,3}(?:[,\d]{0,})+)\s*(?:inr|rs|rupees)?)");
        if (std::regex_search(s,m,rx)){
            string num = m[1];
            // remove commas
            num.erase(remove(num.begin(), num.end(), ','), num.end());
            try { return stoi(num); } catch(...) {}
        }
        // fallback digits
        std::regex rx2(R"((\d{5,7}))");
        if (std::regex_search(s,m,rx2)) return stoi(m[1]);
    } catch(...) {}
    return -1;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    // read all stdin
    string input;
    string all;
    while (getline(cin, input)) { all += input + "\n"; }
    if (all.empty()){
        json e; e["error"]="no-input"; cout << e.dump(); return 1;
    }
    json in;
    try { in = json::parse(all); } catch (...) {
        json e; e["error"]="bad-json"; cout << e.dump(); return 1;
    }

    string query = in.value("query", string());
    vector<string> clauses;
    if (in.contains("policyClauses") && in["policyClauses"].is_array()){
        for (auto &c: in["policyClauses"]) clauses.push_back(c.get<string>());
    }

    ParsedQuery pq = parse_query(query);

    // score clauses
    vector<pair<double,string>> scored;
    for (auto &c: clauses){
        double sc = score_clause(c, pq);
        if (sc>0) scored.push_back({sc,c});
    }
    sort(scored.begin(), scored.end(), [](auto &a, auto &b){ return a.first > b.first; });

    // decision logic
    bool covered = false;
    bool waiting = false;
    int waitingMonths = 0;
    int coverAmount = -1;
    json justification = json::array();
    int topN = min((int)scored.size(), 8);
    for (int i=0;i<topN;i++){
        double sc = scored[i].first;
        string c = scored[i].second;
        justification.push_back({ {"doc_id","policy_sample"}, {"text", c}, {"score", sc} });
        string lc = lower(c);
        if (lc.find("knee")!=string::npos && lc.find("surgery")!=string::npos) covered = true;
        if (lc.find("waiting")!=string::npos){
            waiting = true;
            int m = extract_first_int(lc);
            if (m>0) waitingMonths = max(waitingMonths, m);
        }
        if (lc.find("limit")!=string::npos || lc.find("sum insured")!=string::npos || lc.find("maximum payable")!=string::npos){
            int am = extract_amount(lc);
            if (am>0) coverAmount = max(coverAmount, am);
        }
    }

    json out;
    if (!covered){
        out["Decision"] = "Rejected";
        out["Amount"] = 0;
        out["Reason"] = "Procedure not covered by matched clauses.";
    } else if (waiting && pq.policyMonths>=0 && pq.policyMonths < waitingMonths){
        out["Decision"] = "Rejected";
        out["Amount"] = 0;
        out["Reason"] = "Policy is within waiting period.";
        out["WaitingMonthsRequired"] = waitingMonths;
        out["PolicyMonths"] = pq.policyMonths;
    } else {
        out["Decision"] = "Approved";
        if (coverAmount>0) out["Amount"] = coverAmount;
        else out["Amount"] = "Refer to clause";
        out["Reason"] = "Covered procedure and waiting period satisfied (if any).";
    }
    out["ParsedQuery"] = { {"age", pq.age}, {"sex", pq.sex}, {"procedure", pq.procedure}, {"location", pq.location}, {"policyMonths", pq.policyMonths} };
    out["Justification"] = justification;
    cout << out.dump(2) << endl;
    return 0;
}
