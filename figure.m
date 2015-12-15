%% testcase
X   =   [10, 15, 20, 25, 30, 34, 35, 36];
LRU =   [79, 74, 71, 71, 70, 70, 38, 36];

LFU =   [378,324,239,176,114,64, 51, 36];
% BLD =   [ ,36];
Raw_LFU=[400, 400, 400, 400, 400, 400, 400, 36];

plot(X,LRU,'-*',X,LFU,'-o',X,Raw_LFU,'-s');
xlabel('Cache Size');
ylabel('Miss count');
axis([10,40,30,450]);
set(gca,'FontSize',20);
legend('LRU','LFU','RawLFU');