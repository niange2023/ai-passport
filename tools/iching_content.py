#!/usr/bin/env python3
"""Build the Pocket I-Ching & Oblique Oracle checked-in JSON and C content.

Contains:
1. Complete 64 Hexagrams (六十四卦): King Wen sequence, 6-bit trigram mask
   (bit 0 = 初爻 bottom, bit 5 = 上爻 top), 卦辞, 大象传, 义理心要, and
   all 384 爻辞 + 小象传 (plus 乾用九 and 坤用六).
2. 100 First-Principles Breakthrough Prompts (100条第一性原理破局签).

Usage:
  python3 tools/iching_content.py          # Generate JSON and C files
  python3 tools/iching_content.py --check  # Verify existing JSON and C files
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

JSON_PATH = Path("main/iching_data/iching_oracle.json")
HEADER_PATH = Path("main/iching_content.h")
SOURCE_PATH = Path("main/iching_content.c")

# (wikisource_title, full_cn_title, 6bit_mask_bottom_to_top, plain_summary)
HEX_META = [
    ("乾", "乾为天", 0b111111, "刚健中正，自强不息；审时度势，知进知退。"),
    ("坤", "坤为地", 0b000000, "厚德载物，顺势而为；先立根基，不争先声。"),
    ("屯", "水雷屯", 0b010001, "万物始生，破局维艰；扎根屯聚，借力建侯。"),
    ("蒙", "山水蒙", 0b100010, "启蒙发微，果行育德；澄明初心，循序渐进。"),
    ("需", "水天需", 0b010111, "云上于天，待时而动；饮食宴乐，养精蓄锐。"),
    ("訟", "天水讼", 0b111010, "天水违行，止争息讼；作事谋始，中吉终凶。"),
    ("師", "地水师", 0b000010, "地中有水，行险而顺；师出以律，容民畜众。"),
    ("比", "水地比", 0b010000, "地上有水，亲比辅佐；审慎择盟，诚信相交。"),
    ("小畜", "风天小畜", 0b110111, "风行天上，密云不雨；积小成大，懿文蓄德。"),
    ("履", "天泽履", 0b111011, "履虎尾而不咥，柔履刚也；谨守分际，素履往吉。"),
    ("泰", "地天泰", 0b000111, "天地交而万物通，上下同心；居安思危，裁成辅相。"),
    ("否", "天地否", 0b111000, "天地不交，闭塞不通；俭德辟难，静待转机。"),
    ("同人", "天火同人", 0b111101, "同人于野，志同道合；唯君子为能通天下之志。"),
    ("大有", "火天大有", 0b101111, "火在天上，丰盛大有；遏恶扬善，满而不溢。"),
    ("謙", "地山谦", 0b000100, "地中有山，裒多益寡；谦尊而光，卑而不可逾。"),
    ("豫", "雷地豫", 0b001000, "雷出地奋，顺以动豫；预立章法，勿溺安逸。"),
    ("隨", "泽雷随", 0b011001, "刚来而下柔，随时从势；向晦入宴，懂得以退为进。"),
    ("蠱", "山风蛊", 0b100110, "山下有风，积弊生蛊；振民育德，革除陈疴。"),
    ("臨", "地泽临", 0b000011, "泽上有地，亲临督导；教思无穷，未雨绸缪。"),
    ("觀", "风地观", 0b110000, "风行地上，仰观俯察；省方观民，以身作则。"),
    ("噬嗑", "火雷噬嗑", 0b101001, "颐中有物，果断啮合；明罚敕法，刚柔分明。"),
    ("賁", "山火贲", 0b100101, "山下有火，文质相济；小利攸往，返璞归真。"),
    ("剝", "山地剥", 0b100000, "山附于地，阴盛剥阳；顺而止之，厚下安宅。"),
    ("復", "地雷复", 0b000001, "雷在地中，一阳来复；出入无疾，见天地之心。"),
    ("无妄", "天雷无妄", 0b111001, "天下雷行，真实不妄；尽人事而听天命，不妄求。"),
    ("大畜", "山天大畜", 0b100111, "天在山中，厚积薄发；多识前言往行，以畜其德。"),
    ("頤", "山雷颐", 0b100001, "山下有雷，颐养身心；慎言语，节饮食，自求口实。"),
    ("大過", "泽风大过", 0b011110, "栋桡之象，非常之局；独立不惧，遁世无闷。"),
    ("坎", "坎为水", 0b010010, "水洊至，习坎重险；维心亨，常德行，习教事。"),
    ("離", "离为火", 0b101101, "明两作离，柔丽乎中正；大人以继明照于四方。"),
    ("咸", "泽山咸", 0b011100, "山上有泽，无心之感；虚怀纳受，感而遂通。"),
    ("恒", "雷风恒", 0b001110, "雷风相与，久于其道；立不易方，终则有始。"),
    ("遯", "天山遁", 0b111100, "天下有山，知机退藏；远小人，不恶而严。"),
    ("大壯", "雷天大壮", 0b001111, "雷在天上，刚以动壮；非礼弗履，慎防恃强触藩。"),
    ("晉", "火地晋", 0b101000, "明出地上，顺而丽乎大明；自昭明德，昼日三接。"),
    ("明夷", "地火明夷", 0b000101, "明入地中，韬光养晦；内文明而外柔顺，用晦而明。"),
    ("家人", "风火家人", 0b110101, "风自火出，言有物而行有恒；齐家立序，名正言顺。"),
    ("睽", "火泽睽", 0b101011, "上火下泽，同而异也；求同存异，以小事吉。"),
    ("蹇", "水山蹇", 0b010100, "山上有水，见险而能止；行有不得，反身修德。"),
    ("解", "雷水解", 0b001010, "雷雨作，险难消解；赦过宥罪，夙吉复常。"),
    ("損", "山泽损", 0b100011, "山下有泽，损下益上；惩忿窒欲，减繁就简。"),
    ("益", "风雷益", 0b110001, "风雷相助，损上益下；见善则迁，有过则改。"),
    ("夬", "泽天夬", 0b011111, "泽上于天，果断决裂；健而悦，决而和。"),
    ("姤", "天风姤", 0b111110, "天下有风，不期而遇；微阴始生，防微杜渐。"),
    ("萃", "泽地萃", 0b011000, "泽上于地，聚众汇力；除戎器，戒不虞，聚而有备。"),
    ("升", "地风升", 0b000110, "地中生木，柔以时升；顺德积小，以成高大。"),
    ("困", "泽水困", 0b011010, "泽无水，处困而不失其所亨；致命遂志，尚口乃穷。"),
    ("井", "水风井", 0b010110, "木上有水，改邑不改井；深掘根基，寒泉利民。"),
    ("革", "泽火革", 0b011101, "泽中有火，顺天应人；己日乃孚，革故鼎新。"),
    ("鼎", "火风鼎", 0b101110, "木上有火，烹饪成新；正位凝命，选贤任能。"),
    ("震", "震为雷", 0b001001, "洊雷震惊，恐惧修省；震来虩虩，处变不惊。"),
    ("艮", "艮为山", 0b100100, "兼山艮，时止则止，时行则行；思不出其位。"),
    ("漸", "风山渐", 0b110100, "山上有木，鸿渐于陆；居贤德善俗，循序渐进。"),
    ("歸妹", "雷泽归妹", 0b001011, "泽上有雷，永终知敝；审视名分，勿以情害义。"),
    ("豐", "雷火丰", 0b001101, "雷电皆至，盛大丰盈；日中则昃，勿忧宜日中。"),
    ("旅", "火山旅", 0b101100, "山上有火，羁旅慎行；柔得中乎外，明慎不留。"),
    ("巽", "巽为风", 0b110110, "随风巽，柔顺深入；申命行事，相机而动。"),
    ("兌", "兑为泽", 0b011011, "丽泽兑，朋友讲习；外柔内刚，以诚相悦。"),
    ("渙", "风水涣", 0b110010, "风行水上，涣散破执；凝聚人心，利涉大川。"),
    ("節", "水泽节", 0b010011, "泽上有水，制数度议德行；苦节不可贞，知止有度。"),
    ("中孚", "风泽中孚", 0b110011, "泽上有风，信及豚鱼；外实内虚，至诚感通。"),
    ("小過", "雷山小过", 0b001100, "山上有雷，宜下不宜上；行过乎恭，用过乎俭。"),
    ("既濟", "水火既济", 0b010101, "水在火上，大功初成；初吉终乱，思患而预防之。"),
    ("未濟", "火水未济", 0b101010, "火在水上，生生不息；慎辨物居方，新局之始。"),
]

# 100 First-Principles Breakthrough Prompts (100条第一性原理破局签)
ORACLE_PROMPTS = [
    # 一、追本与第一性原理（01-20）
    ("追本·真问题", "如果允许你把眼前这件事彻底不做，最坏的真实后果究竟是什么？"),
    ("追本·去包装", "剥掉所有行业黑话和漂亮名词，这件事用大白话讲到底在交换什么？"),
    ("追本·物理约束", "框住你的到底是客观物理定律，还是别人随口定下的旧惯例？"),
    ("追本·十年尺度", "放到十年后再回头看今天，你现在纠结的这件事还剩几分重量？"),
    ("追本·最小闭环", "如果只给你两小时和一张白纸，这件事最核心的最小验证动作是什么？"),
    ("追本·逆向归因", "假设半年后这个方案彻底失败了，写下尸检报告里最可能的三条死因。"),
    ("追本·砍掉一半", "如果必须砍掉当前方案里一半的步骤，你会留下哪一半？为什么现在不直接只做那一半？"),
    ("追本·唯一抓手", "在十几个看似都重要的现象背后，真正决定全局的唯一独立变量是哪个？"),
    ("追本·目的与手段", "你是不是把原本为了解决问题的「手段」，不知不觉当成了必须维护的「目的」？"),
    ("追本·沉没成本", "假如你今天第一天接手这个摊子、没有任何历史包袱，你还会继续投吗？"),
    ("追本·底线思维", "如果眼前这条路被证明走不通，你真正在保护的核心底线是什么？"),
    ("追本·证据分级", "你现在的焦虑，是基于已经发生的确认事实，还是基于脑补的推演？"),
    ("追本·零基重来", "如果现有的工作流全被推平，从最终交付物倒推，最短路径只有哪三步？"),
    ("追本·谁的课题", "这件事的最终后果到底由谁承担？你是不是把别人的课题背到了自己身上？"),
    ("追本·慢就是快", "在还没有定义清楚「什么叫做好」之前，跑得越快是不是返工越惨？"),
    ("追本·反例检验", "有没有哪怕一个真实反例，能直接证伪你现在最笃定的那个前提？"),
    ("追本·能量账本", "做完这件事之后，你的心力是会增加还是被抽干？这笔能量账划算吗？"),
    ("追本·不做什么", "今天明确决定「不做什么」，比再往待办清单里塞三件事更有力量。"),
    ("追本·长期复利", "此刻耗费心力的动作，半年后能留下可复用的方法、代码或口碑吗？"),
    ("追本·直面核心", "你一直在打磨外围细节，是不是为了逃避最难但最关键的那次正面对话？"),

    # 二、职场博弈与组织看透（21-40）
    ("博弈·利益底座", "不要听对方在会上说了什么冠冕堂皇的话，去算这个方案动了谁的什么利益。"),
    ("博弈·承认正当", "承认对方的立场有其合理性，不会消除你的损失，但能让你立刻脱离情绪缠斗去算真账。"),
    ("博弈·向上对齐", "老板嘴上要的是一个工具，他心里真正要交差的那份「安全感」或「战功」是什么？"),
    ("博弈·预沟通", "在把方案端上大会桌面之前，最可能投反对票的那个人，你私下通气了吗？"),
    ("博弈·责任边界", "当所有人都在说「我们要协同」时，出了事究竟谁的名字写在第一责任人上？"),
    ("博弈·权力流向", "在这个组织里，真正的奖惩权、资源分配权和信息解释权握在谁手里？"),
    ("博弈·不替背锅", "下属反复掉链子，究竟是人的能力问题，还是糟糕的系统环境在逼好人做蠢事？"),
    ("博弈·留出退路", "给对方留一个体面的台阶下，往往比在道理上彻底辩赢他更能拿到结果。"),
    ("博弈·非对称优势", "在这场博弈里，什么是你独有而别人短期内抄不走的筹码？"),
    ("博弈·规则与例外", "对方是在按正式规则办事，还是在利用规则的模糊地带试探你的边界？"),
    ("博弈·亮出底牌", "温和但坚定地亮出你的底线，远比委曲求全后再爆发更能赢得尊重。"),
    ("博弈·谁在买单", "分清楚谁是使用者、谁是挑刺者、谁是真正的拍板买单人。"),
    ("博弈·增量破局", "存量博弈里人人都在抢蛋糕；能不能切出一块谁都还没碰过的增量样板？"),
    ("博弈·少说多看", "在局势未明的新环境里，先交付一个无可挑剔的小确定性，胜过十次宏大表态。"),
    ("博弈·预期管理", "把承诺锁在八分，把交付做到十分；永远不要用十分的满话去赌八分的把握。"),
    ("博弈·借力打力", "与其凭一己之力硬推石头上山，不如看看哪股现成的大势顺路能带你一程。"),
    ("博弈·书面留痕", "口头共识会随上下文切换而蒸发；关键取舍与边界，散会前用三行字落盘了吗？"),
    ("博弈·停止自证", "当对方一开始就打算给你贴标签时，自证清白就是跳进他设好的框架。"),
    ("博弈·换位一秒", "如果你坐在对方那个位置、背着他的考核指标，你也会做出同样讨厌的决定吗？"),
    ("博弈·全身而退", "在形势明显不可为的局部战场，保存实力与口碑比硬撑面子重要得多。"),

    # 三、纵向心智与内在觉察（41-60）
    ("心智·跳出运筹", "你现在非要争个高下、改到完美，是事情本身需要，还是「运筹者」的自尊心受不了？"),
    ("心智·主体转客体", "当你能指着自己的情绪说「我注意到我有点急」时，你就不再被这股急躁控制了。"),
    ("心智·不讨好", "如果你完全不在乎对方此刻高不高兴，你真正想说的那句实话是什么？"),
    ("心智·容忍灰度", "这件事一定非黑即白吗？能不能允许两个看似矛盾的真相同时成立？"),
    ("心智·放下控制", "你越想严丝合缝地控制每一个环节，系统里其他人的主动性就越快萎缩。"),
    ("心智·区分评价", "别人的一句负面反馈，只是对这件具体产出的观察，并不等于对你整个人格的判决。"),
    ("心智·觉察防御", "刚才你心里升起的那股反驳冲动，到底是在保卫真相，还是在保卫面子？"),
    ("心智·允许未完成", "允许今天带着一点「还没彻底想通」去睡觉，天不会塌下来。"),
    ("心智·身体信号", "停下脑子里的推理三秒钟：此刻你的肩膀、胃部和呼吸是紧绷还是舒展的？"),
    ("心智·不强求一致", "过去的你说的判断，不必成为绑架今天你的枷锁；认知升级本来就允许自我修正。"),
    ("心智·看见阴影", "你最看不惯某个人身上的那个缺点，是不是恰恰是你自己极力压抑的那一面？"),
    ("心智·从容留白", "把日程表塞满只会带来虚假的勤奋感；真正的战略级洞见都长在留白里。"),
    ("心智·慈悲而清醒", "对人的处境保持最大的悲悯，同时对事的规律保持最冷酷的诚实。"),
    ("心智·不作救世主", "每个人都有自己要走的弯路，克制住替别人包办人生的冲动。"),
    ("心智·接纳脆弱", "坦然承认「这块我目前确实不懂」，反而比硬撑专家架子更有力量。"),
    ("心智·重回当下", "过去已成记忆，未来尚属推演；此时此刻你手头唯一能踏实做完的一件小事是什么？"),
    ("心智·无住生心", "全力以赴去推进，但把对「必须立刻看到唯一结果」的执念轻轻放开。"),
    ("心智·观察者视角", "如果把今天这幕冲突当成《生活电视台》里的一集像素剧，旁白会怎么温和地调侃这一幕？"),
    ("心智·睡眠优先", "任何在深夜十一点后看起来天崩地裂的难题，先睡饱八小时再看，往往只剩一半。"),
    ("心智·一致与弘毅", "今天做的这个选择，让你的内外更一致了，还是更分裂了？"),

    # 四、系统工程与 AI 时代的取舍（61-80）
    ("工程·风险压缩", "别让 AI 直接替你做最终裁决；让它负责聚类、找冲突、列证据，拍板留给人与确定性规则。"),
    ("工程·独立审计", "生成方案和验证方案不能混在同一个上下文里；换一把独立尺子做验收。"),
    ("工程·可回滚单元", "不要追求一次性全量做对；把变更拆成随时能观察、能叫停、能秒级回滚的最小单元。"),
    ("工程·状态恢复", "省下内存或资源只是前半场，系统异常重启后能不能无损恢复现场才是真闭环。"),
    ("工程·马车与汽车", "关键不是追逐每一个新出的 AI 玩具，而是怎样把你在「马车时代」的深厚经验迁移到「汽车架构」上。"),
    ("工程·重组工作流", "不要在旧的古法流程里勉强塞一个 AI 插件；从 Agent 的能力边界出发重新设计整条流水线。"),
    ("工程·先测证伪", "如果一个便宜的十分钟小实验就能证伪这个假设，绝不要先写三天完美架构。"),
    ("工程·单一真相源", "当两份文档或两个看板迟早会不一致时，果断砍掉派生副本，只留唯一真相源。"),
    ("工程·触点递减", "好的自动化应该越来越隐形；如果一个新工具每天都需要你去伺候它，立刻停掉它。"),
    ("工程·瓶颈定位", "在没有看清全链路真正的卡脖子环节之前，局部优化得越极致，整体浪费越大。"),
    ("工程·降级预案", "当外部接口、网络或模型突然不可用时，系统能不能优雅退回本地兜底模式？"),
    ("工程·拒绝过度设计", "不要为了未来只有百分之一概率出现的假想需求，把今天的代码写复杂三倍。"),
    ("工程·用数据说话", "把「我觉得变快了/变好了」改写成一条可重复执行的基准测试与前后对比指标。"),
    ("工程·控制变量", "一次同时改五个参数，就算结果变好了你也根本不知道是谁起的作用。"),
    ("工程·边界清晰", "模块之间最好的协作不是共享一切，而是只通过最窄、最稳定的契约通信。"),
    ("工程·故障注入", "与其祈祷线上永远不出错，不如现在亲手拔掉一根线，看看系统怎么报错。"),
    ("工程·工具归工具", "锤子再精致也只是锤子；永远围绕最终要交付的业务价值，而不是自娱自乐地磨锤子。"),
    ("工程·消灭僵尸项", "定期清理那些食之无味、弃之可惜的半成品分支与僵尸看板，给主线腾出心智带宽。"),
    ("工程·人机分工", "把重复搬运和全量扫描彻底交给机器，把极其稀缺的人类注意力留给取舍与审美。"),
    ("工程·完结优先", "一个已经上线并跑通真实闭环的 70 分样板，胜过十个躺在本地分支里的 99 分半成品。"),

    # 五、创作、表达与破局行动（81-100）
    ("破局·具体场景", "把抽象的大道理全部删掉，回到一个具体的人、在具体的一天、说出的那句具体原话。"),
    ("破局·反直觉切口", "大家都顺着说的常识不用你再写一遍；这件事里最违背直觉但千真万确的事实是什么？"),
    ("破局·只讲透一点", "一篇文章、一次汇报甚至一个硬件，整场只让人死死记住一个核心洞察就够了。"),
    ("破局·给对方抓手", "别人读完或听完之后，接下来五分钟内就能立刻照着做的一个微小动作是什么？"),
    ("破局·先写丑初稿", "卡住写不出开头时，先允许自己写一段最烂的大白话口水稿；有沙子才能淘出金子。"),
    ("破局·换个听众", "假如坐在你对面的是十年前刚当管理者的你自己，你会怎么跟他说这件事？"),
    ("破局·比喻降维", "能不能找一个日常生活中人人见过的东西（如焖面、怀表、手扶拖拉机），把复杂机制一秒讲穿？"),
    ("破局·删掉开场白", "直接把你写的第三段提上来当第一句，看看是不是一下子就有劲了？"),
    ("破局·真实伤疤", "完美的成功经验让人敬而远之，真实的踩坑、犹豫和修正过程才能让人产生共鸣。"),
    ("破局·口碑思维", "别去想怎么打造虚无缥缈的「个人品牌」，去想这件事做完后别人私下会怎么评价你的「口碑」。"),
    ("破局·主动破冰", "与其在座位上猜对方怎么想，不如现在起身走过去或者发条消息，直接聊五分钟。"),
    ("破局·改变物理场", "如果坐在电脑前已经卡壳超过半小时，合上盖子，揣着这支签下楼走五百米。"),
    ("破局·小步试错", "把「我要做成一个大产品」改成「我今天先让一个真实用户跑通第一单」。"),
    ("破局·逆向提问", "如果完全反过来做——别人做加法你做减法，别人联网你断网——会发生什么奇妙反应？"),
    ("破局·借假修真", "外在的项目、文章和指标都是磨刀石；借着这件具体的事，你想修出自己身上的什么功夫？"),
    ("破局·敢于收尾", "承认一件事已经试够了、可以体面画上句号，是比盲目坚持更高级的决断力。"),
    ("破局·留住火种", "把刚才一闪而过的那个念头立刻用一句话记下来，别等它被下一条消息冲走。"),
    ("破局·回归常识", "当所有复杂的模型都互相打架时，回到最朴素的人性常识：人们总是趋利避害、喜欢简单。"),
    ("破局·今日一剑", "如果今天只做成一件事就能心满意足地收工，那件事是什么？现在就去干它。"),
    ("破局·生生不息", "《周易》以「未济」收尾：没有永远大功告成的终局，每一次收官都是下一场探索的初爻。"),
]


def convert_to_simplified(trad_text: str) -> str:
    """Use macOS Foundation StringTransform to convert Traditional Chinese to Simplified,
    then fix classic I-Ching specific characters (乾, 坤, 徵, 祇, 后, 历, 蒙, etc.)."""
    script = (
        'import Foundation\n'
        'let data = FileHandle.standardInput.readDataToEndOfFile()\n'
        'let s = String(data: data, encoding: .utf8)! as NSString\n'
        'let out = s.applyingTransform(.init("Traditional-Simplified"), reverse: false)!\n'
        'FileHandle.standardOutput.write(out.data(using: .utf8)!)\n'
    )
    res = subprocess.run(
        ["swift", "-e", script],
        input=trad_text.encode("utf-8"),
        capture_output=True,
        check=True,
    )
    text = res.stdout.decode("utf-8")
    # Fix classic I-Ching characters where automatic T->S over-converts "乾" -> "干"
    fixes = [
        ("干：", "乾："),
        ("干，", "乾，"),
        ("干元", "乾元"),
        ("干道", "乾道"),
        ("干干", "乾乾"),
        ("干下", "乾下"),
        ("干上", "乾上"),
        ("周易/干", "周易/乾"),
        ("群龙", "群龙"),
        ("无咎", "无咎"),
        ("无妄", "无妄"),
        ("无不利", "无不利"),
        ("无攸利", "无攸利"),
    ]
    for old, new in fixes:
        text = text.replace(old, new)
    return text


def clean_wiki_line(line: str) -> str:
    line = re.sub(r"\{\{另\|([^|}]+)\|[^}]+\}\}", r"\1", line)
    line = re.sub(r"\{\{\*\|[^}]+\}\}", "", line)
    line = re.sub(r"-\{([^|}]+)\}-", r"\1", line)
    line = re.sub(r"<[^>]+>", "", line)
    line = re.sub(r"'''", "", line)
    return line.strip()


def fetch_and_build_dataset() -> dict:
    raw_pages = {}
    for i in range(0, 64, 16):
        batch = ["周易/" + item[0] for item in HEX_META[i : i + 16]]
        url = (
            "https://zh.wikisource.org/w/api.php?action=query&prop=revisions"
            "&rvprop=content&format=json&titles="
            + urllib.parse.quote("|".join(batch))
        )
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        data = json.loads(urllib.request.urlopen(req, timeout=20).read().decode("utf-8"))
        for _pid, page in data["query"]["pages"].items():
            title_part = page["title"].split("/")[1]
            raw_pages[title_part] = page["revisions"][0]["*"]

    # Join all pages with delimiter to run Swift conversion in a single fast invocation
    joined_trad = "\n===HEX_SPLIT===\n".join(raw_pages[item[0]] for item in HEX_META)
    joined_simp = convert_to_simplified(joined_trad)
    simp_pages = joined_simp.split("\n===HEX_SPLIT===\n")
    assert len(simp_pages) == 64

    hexagrams = []
    for idx, (wiki_name, full_title, mask, summary) in enumerate(HEX_META):
        wt = simp_pages[idx]
        lines = [clean_wiki_line(l) for l in wt.splitlines() if l.strip()]

        in_yijing = False
        in_xiang = False
        guaci = ""
        yaoci = []
        daxiang = ""
        xiaoxiang = []

        for l in lines:
            if "易经：" in l or "易經：" in l:
                in_yijing = True
                in_xiang = False
                continue
            if "彖曰：" in l:
                in_yijing = False
                in_xiang = False
                continue
            if "象曰：" in l:
                in_yijing = False
                in_xiang = True
                continue
            if "文言曰：" in l:
                in_yijing = False
                in_xiang = False
                continue

            if in_yijing:
                if l.startswith("**"):
                    guaci = l.lstrip("*").strip()
                elif l.startswith("*#"):
                    yaoci.append(l.lstrip("*#").strip())
            elif in_xiang:
                if l.startswith("**"):
                    daxiang = l.lstrip("*").strip()
                elif l.startswith("*#"):
                    xiaoxiang.append(l.lstrip("*#").strip())

        if "：" in guaci:
            short_name, guaci_body = guaci.split("：", 1)
        else:
            short_name, guaci_body = wiki_name, guaci

        assert len(yaoci) >= 6, f"Hexagram {idx+1} has <6 yaoci"
        assert len(xiaoxiang) >= 6, f"Hexagram {idx+1} has <6 xiaoxiang"

        six_lines = []
        for line_i in range(6):
            raw_y = yaoci[line_i]
            if "：" in raw_y:
                y_label, y_text = raw_y.split("：", 1)
            else:
                y_label, y_text = f"第{line_i+1}爻", raw_y
            six_lines.append(
                {
                    "pos": line_i + 1,
                    "label": y_label.strip(),
                    "text": y_text.strip(),
                    "xiang": xiaoxiang[line_i].strip(),
                }
            )

        extra_line = ""
        if len(yaoci) > 6:
            extra_line = yaoci[6].strip()

        hexagrams.append(
            {
                "number": idx + 1,
                "name": short_name.strip(),
                "title": full_title,
                "mask": mask,
                "judgment": guaci_body.strip(),
                "image": daxiang.strip(),
                "summary": summary,
                "extra": extra_line,
                "lines": six_lines,
            }
        )

    assert len(ORACLE_PROMPTS) == 100
    prompts = [
        {"id": i + 1, "category": cat, "question": q}
        for i, (cat, q) in enumerate(ORACLE_PROMPTS)
    ]

    return {
        "schema_version": 1,
        "hexagram_count": 64,
        "line_count": 384,
        "prompt_count": 100,
        "hexagrams": hexagrams,
        "prompts": prompts,
    }


def c_escape(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
    )


def emit_c_files(data: dict) -> tuple[str, str]:
    header = """// main/iching_content.h
// Auto-generated by tools/iching_content.py. Do not edit by hand.
// Complete 64 Hexagrams, 384 Lines (周易六十四卦三百八十四爻), and 100 First-Principles Prompts.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define ICHING_HEXAGRAM_COUNT 64U
#define ICHING_LINE_COUNT     6U
#define ICHING_TOTAL_LINES    384U
#define ICHING_PROMPT_COUNT   100U

typedef struct {
    const char *label;  // 初九 / 六二 ...
    const char *text;   // 爻辞原文
    const char *xiang;  // 小象传原文
} iching_line_t;

typedef struct {
    uint8_t number;         // 周易卦序 (1..64)
    uint8_t mask;           // 6-bit 卦画掩码 (bit0=初爻 .. bit5=上爻, 1=阳, 0=阴)
    const char *name;       // 卦名 (如 "乾"、"蹇")
    const char *title;      // 完整卦题 (如 "乾为天"、"水山蹇")
    const char *judgment;   // 卦辞原文
    const char *image;      // 大象传原文
    const char *summary;    // 现代义理心要
    const char *extra;      // 用九 / 用六 (其余卦为空字符串)
    iching_line_t lines[ICHING_LINE_COUNT];
} iching_hexagram_t;

typedef struct {
    uint8_t id;             // 签号 (1..100)
    const char *category;   // 维度标签 (如 "追本·真问题")
    const char *question;   // 第一性原理破局提问
} iching_prompt_t;

const iching_hexagram_t *iching_get_hexagram(uint8_t index);
const iching_hexagram_t *iching_find_by_mask(uint8_t mask6);
uint8_t iching_index_by_mask(uint8_t mask6);
const iching_prompt_t *iching_get_prompt(uint8_t index);
"""

    src_lines = [
        "// main/iching_content.c",
        "// Auto-generated by tools/iching_content.py. Do not edit by hand.",
        '#include "iching_content.h"',
        "",
        "static const iching_hexagram_t k_hexagrams[ICHING_HEXAGRAM_COUNT] = {",
    ]

    for h in data["hexagrams"]:
        src_lines.append("    {")
        src_lines.append(f"        .number = {h['number']}U,")
        src_lines.append(f"        .mask = 0x{h['mask']:02X}U,")
        src_lines.append(f'        .name = "{c_escape(h["name"])}",')
        src_lines.append(f'        .title = "{c_escape(h["title"])}",')
        src_lines.append(f'        .judgment = "{c_escape(h["judgment"])}",')
        src_lines.append(f'        .image = "{c_escape(h["image"])}",')
        src_lines.append(f'        .summary = "{c_escape(h["summary"])}",')
        src_lines.append(f'        .extra = "{c_escape(h["extra"])}",')
        src_lines.append("        .lines = {")
        for ln in h["lines"]:
            src_lines.append(
                f'            {{ "{c_escape(ln["label"])}", "{c_escape(ln["text"])}", "{c_escape(ln["xiang"])}" }},'
            )
        src_lines.append("        },")
        src_lines.append("    },")

    src_lines.append("};")
    src_lines.append("")
    src_lines.append("static const iching_prompt_t k_prompts[ICHING_PROMPT_COUNT] = {")
    for p in data["prompts"]:
        src_lines.append(
            f'    {{ {p["id"]}U, "{c_escape(p["category"])}", "{c_escape(p["question"])}" }},'
        )
    src_lines.append("};")
    src_lines.append("")
    src_lines.append(
        """const iching_hexagram_t *iching_get_hexagram(uint8_t index)
{
    if (index >= ICHING_HEXAGRAM_COUNT) return &k_hexagrams[0];
    return &k_hexagrams[index];
}

uint8_t iching_index_by_mask(uint8_t mask6)
{
    uint8_t masked = (uint8_t)(mask6 & 0x3FU);
    for (uint8_t i = 0; i < ICHING_HEXAGRAM_COUNT; i++) {
        if (k_hexagrams[i].mask == masked) {
            return i;
        }
    }
    return 0;
}

const iching_hexagram_t *iching_find_by_mask(uint8_t mask6)
{
    return &k_hexagrams[iching_index_by_mask(mask6)];
}

const iching_prompt_t *iching_get_prompt(uint8_t index)
{
    if (index >= ICHING_PROMPT_COUNT) return &k_prompts[0];
    return &k_prompts[index];
}
"""
    )

    return header, "\n".join(src_lines)


def check_existing() -> int:
    if not JSON_PATH.exists() or not HEADER_PATH.exists() or not SOURCE_PATH.exists():
        print("Missing generated I-Ching files", file=sys.stderr)
        return 1
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    if len(data.get("hexagrams", [])) != 64:
        print("Expected 64 hexagrams", file=sys.stderr)
        return 1
    if len(data.get("prompts", [])) != 100:
        print("Expected 100 prompts", file=sys.stderr)
        return 1
    masks = set()
    total_lines = 0
    for h in data["hexagrams"]:
        masks.add(h["mask"])
        lines = h.get("lines", [])
        if len(lines) != 6:
            print(f"Hexagram {h['number']} does not have 6 lines", file=sys.stderr)
            return 1
        total_lines += len(lines)
    if len(masks) != 64 or total_lines != 384:
        print("Mask or line count mismatch", file=sys.stderr)
        return 1
    header, source = emit_c_files(data)
    if HEADER_PATH.read_text(encoding="utf-8") != header:
        print("iching_content.h is out of sync with JSON", file=sys.stderr)
        return 1
    if SOURCE_PATH.read_text(encoding="utf-8") != source:
        print("iching_content.c is out of sync with JSON", file=sys.stderr)
        return 1
    print("I-Ching content check: PASS (64 hexagrams, 384 lines, 100 prompts)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify generated files")
    args = parser.parse_args()

    if args.check:
        return check_existing()

    if JSON_PATH.exists():
        data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    else:
        data = fetch_and_build_dataset()
        JSON_PATH.parent.mkdir(parents=True, exist_ok=True)
        JSON_PATH.write_text(
            json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )

    header, source = emit_c_files(data)
    HEADER_PATH.write_text(header, encoding="utf-8")
    SOURCE_PATH.write_text(source, encoding="utf-8")
    return check_existing()


if __name__ == "__main__":
    raise SystemExit(main())
