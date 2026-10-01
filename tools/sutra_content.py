#!/usr/bin/env python3
"""Build the Diamond Sutra application's checked-in JSON and C content.

The scripture text is the public-domain Kumarajiva translation, divided into
the traditional 32 sections. The explanations and practices are original,
short study prompts for this application, not doctrinally authoritative notes.
"""

from __future__ import annotations

import argparse
import html
import json
import math
import re
import urllib.request
from pathlib import Path


SOURCE_URL = "http://www.guoxue123.com/zhibu/0101/0400/403.htm"
PLAIN_PAGES_PATH = "main/sutra_data/diamond_sutra_plain_pages.json"
PLAIN_PAGE_CHAR_LIMIT = 108
COMPARE_PART_CHAR_LIMIT = 42
NUMERALS = "一二三四五六七八九十"

NOTES = [
    ("法会因由", "饭食讫，收衣钵，洗足已，敷座而坐。",
     "经文从吃饭、洗足、坐下写起。般若不离日用，觉察也不必等一个特别时刻。",
     "做完眼前一件小事再开始下一件，不让心比身体先跑远。"),
    ("善现启请", "云何应住？云何降伏其心？",
     "须菩提问的不是如何控制世界，而是心安住在哪里、散乱时如何回来。",
     "今天心乱时先不解决事情，只辨认此刻心抓住了什么。"),
    ("大乘正宗", "实无众生得灭度者。",
     "真正的帮助不把对方变成自己的功劳，也不把帮助者的身份越做越重。",
     "帮一个人完成一件事，事后不反复索取感谢或证明。"),
    ("妙行无住", "应无所住，行于布施。",
     "行动可以坚定，心却不必黏在回报、评价和自我形象上。无住不是不做，而是做后不占有。",
     "做一件有益的事，不预设对方必须怎样回应。"),
    ("如理实见", "凡所有相，皆是虚妄。",
     "相不是不存在，而是不等于全部真相。看见标签、角色和第一印象的有限，才可能看见人和事。",
     "对一个笃定判断补一句：我还没看到什么？"),
    ("正信希有", "不应取法，不应取非法。",
     "执著规则会僵化，反对一切规则也会成为另一种执著。方法是渡河的筏，不是要背一生的身份。",
     "检查一个曾经有效的方法，它是否仍适合眼前情境。"),
    ("无得无说", "无有定法，如来可说。",
     "真实经验不能被一句固定答案封死。语言能指路，但不能替你走路。",
     "把今天最想求标准答案的问题，改写成一个可观察的实验。"),
    ("依法出生", "所谓佛法者，即非佛法。",
     "名称方便我们交流，却不能替代它指向的实践。把名字当实体，理解就停在了概念上。",
     "选一个熟悉概念，用自己的一个具体行为重新解释它。"),
    ("一相无相", "实无有法名阿罗汉。",
     "一旦把成就变成自我标签，成就就可能成为新的束缚。抵达不是占有一个更高身份。",
     "完成一件好事后，只记录事实，不给自己加封号。"),
    ("庄严净土", "应无所住而生其心。",
     "不住并非冷漠和停滞。心不被对象绑住，反而能持续生起清明而有力量的行动。",
     "带着一个重要愿望行动，同时放下对唯一结果的执拗。"),
    ("无为福胜", "受持四句偈等，为他人说。",
     "知识的价值不只在拥有多少，还在是否进入生命、能否帮助别人看见。",
     "把今天读到的一句，用自己的话讲给一个真实的人听。"),
    ("尊重正教", "若是经典所在之处，即为有佛。",
     "敬重不只朝向形式，也朝向能让人醒来的言语和场域。你怎样对待注意力，就怎样塑造心。",
     "为十分钟阅读清出一个不被消息打断的空间。"),
    ("如法受持", "是经名为金刚般若波罗蜜。",
     "金刚喻其能断，般若是照见实相的智慧，波罗蜜是到彼岸。名字是入口，实践才是抵达。",
     "遇到烦恼时分辨：事实是什么，我附加的故事是什么？"),
    ("离相寂灭", "应生无所住心。",
     "忍辱不是压抑痛苦，而是不让受伤立即固化成我、他人和报复的故事。",
     "被冒犯时延迟一次反击，先描述身体感受而不评价对方。"),
    ("持经功德", "受持读诵，为人解说。",
     "读、记、理解、表达是四个不同层次。只有经过转述和行动，文字才成为自己的认识。",
     "读完一段后合上屏幕，用三句话复述其意。"),
    ("能净业障", "是经义不可思议，果报亦不可思议。",
     "遭遇轻贱时，不必把眼前评价判作自我的终局。长期改变常从难以量化的细小转向开始。",
     "面对一次否定，区分可修正的行为与不必接住的人格评判。"),
    ("究竟无我", "一切法无我、无人、无众生、无寿者。",
     "无我不是否认人的存在，而是看见身份由关系和条件构成，没有一个永远固定的自我。",
     "写下一个困住你的身份，再列出三个它不成立的时刻。"),
    ("一体同观", "过去心不可得，现在心不可得，未来心不可得。",
     "心念不停迁流。过去已变成记忆，未来仍是想象，当下也在说出时流逝。",
     "焦虑升起时，只命名眼前能直接感知的三样东西。"),
    ("法界通化", "以福德无故，如来说得福德多。",
     "若把善行换算成可占有的积分，心就被积分缩小。正因为不把福德当实体，善才没有边界。",
     "做完一件善意小事，不在心里记账。"),
    ("离色离相", "不应以具足色身见。",
     "外在完整、权威形象和漂亮表达都不能保证真实。判断要穿过包装，回到它实际造成的影响。",
     "评价一个人或方案时，先隐藏头衔和包装，只看行为与结果。"),
    ("非说所说", "说法者，无法可说，是名说法。",
     "好的表达不把听者锁进结论，而是让他亲自看见。说者也不必把话占为真理。",
     "给建议前先问一个能让对方自行观察的问题。"),
    ("无法可得", "乃至无有少法可得。",
     "智慧不是多拿到一个对象。越想把它变成私有成果，越容易错过它正在改变观看方式。",
     "学习后不急着收藏结论，先观察它改变了哪个判断。"),
    ("净心行善", "是法平等，无有高下。",
     "平等不是抹平差异，而是不从固定自我出发给生命排出本质高下。善行也不该成为优越感。",
     "今天与一个容易忽略的人完整说一次话。"),
    ("福智无比", "受持读诵，为他人说。",
     "物质给予解决一时之需，智慧若被真正理解，可能改变持续做选择的方式。",
     "分享一个方法时，同时说清它的适用边界。"),
    ("化无所化", "实无有众生如来度者。",
     "把自己想成拯救者，会削弱他人的主体性。帮助的完成，是对方重新拥有自己的力量。",
     "少替一个人做决定，多帮他看清可选项和代价。"),
    ("法身非相", "若以色见我，以音声求我，是人行邪道，不能见如来。",
     "形象与声音可以承载真实，却不等于真实。迷恋权威的外观，会错过其所指。",
     "听一个观点时暂时忘掉说话者是谁，只审查证据和逻辑。"),
    ("无断无灭", "于法不说断灭相。",
     "不执著存在，不等于否认因果和价值。空不是虚无，而是事物依条件变化，因此仍可行动。",
     "面对无常，不问还有没有意义，改问我能创造什么条件。"),
    ("不受不贪", "菩萨所作福德，不应贪著。",
     "贡献可以被看见，但不必靠占有贡献维持自我。放下功劳，行动才不会被奖赏反向控制。",
     "完成一次贡献后，把注意力还给下一步需要。"),
    ("威仪寂净", "无所从来，亦无所去。",
     "我们习惯把变化切成到来和离去，却忽略一切都在条件中连续生成。看见过程，得失会松动。",
     "观察一次情绪从生起到消退，不替它寻找永久原因。"),
    ("一合理相", "一合相者，即是不可说。",
     "整体由无数条件暂时聚合。语言可以命名整体，却无法把复杂关系压成一个永恒实体。",
     "拆开一个笼统判断，列出至少三个共同造成它的条件。"),
    ("知见不生", "于一切法，应如是知，如是见，如是信解，不生法相。",
     "知见不是新的牢笼。真正的理解保持开放，能用一个观点，也能在条件变化时放下它。",
     "为一个坚定观点写出它会失效的边界。"),
    ("应化非真", "一切有为法，如梦幻泡影，如露亦如电，应作如是观。",
     "有为法因缘而生，真实发生却不能久住。无常不是贬低世界，而是提醒我们清醒投入、及时放下。",
     "在今天结束前问：若这件事终会变化，我现在最该珍惜什么？"),
]

def clean_text(value: str) -> str:
    value = re.sub(r"<br\s*/?>", "\n", value, flags=re.I)
    value = re.sub(r"<[^>]+>", "", value)
    value = html.unescape(value).replace("\xa0", " ").replace("\u3000", " ")
    value = value.replace("\r", "\n")
    value = re.sub(r"[ \t]+", " ", value)
    value = re.sub(r"\n+", "\n", value)
    return value.strip()


def fetch_sections(url: str) -> list[tuple[str, str]]:
    request = urllib.request.Request(url, headers={"User-Agent": "AI-Passport-Sutra/1.0"})
    raw = urllib.request.urlopen(request, timeout=30).read()
    document = raw.decode("gb18030")
    body = clean_text(document)
    marker = re.compile(r"○([^○\n]{2,20}分第[" + NUMERALS + r"]{1,3})")
    matches = list(marker.finditer(body))
    if len(matches) != 32:
        raise SystemExit(f"expected 32 sections, found {len(matches)}")

    result = []
    for index, match in enumerate(matches):
        title = match.group(1).strip()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(body)
        text = body[match.end():end].strip()
        text = re.sub(r"\s+", "", text)
        if index == 31:
            text = text.split("Poweredby", 1)[0]
        result.append((title, text))
    return result


def mask_text(text: str, level: int) -> str:
    output = []
    visible_index = 0
    punctuation = set("，。！？；：、“”‘’（）《》")
    for char in text:
        if char in punctuation or char.isspace():
            output.append(char)
            if char in "，。！？；":
                visible_index = 0
            continue
        show = (level == 1 and visible_index % 3 != 1) or (level == 2 and visible_index == 0)
        output.append(char if show else "＿")
        visible_index += 1
    return "".join(output)


def exact_highlight(text: str, key: str) -> str:
    ignored = set("，。！？；：、“”‘’（）《》,.!?;: \n")
    normalized_text = []
    source_offsets = []
    for offset, char in enumerate(text):
        if char not in ignored:
            normalized_text.append(char)
            source_offsets.append(offset)
    normalized_key = "".join(char for char in key if char not in ignored)
    start = "".join(normalized_text).find(normalized_key)
    if start < 0:
        raise SystemExit(f"key sentence not found in source text: {key}")
    first = source_offsets[start]
    last = source_offsets[start + len(normalized_key) - 1] + 1
    return text[first:last]


def paginate(text: str, limit: int = 72) -> list[str]:
    sentences = re.findall(r".+?[。！？；]|.+$", text)
    pages: list[str] = []
    current = ""
    for sentence in sentences:
        while len(sentence) > limit:
            room = limit - len(current)
            if room > 12:
                current += sentence[:room]
                sentence = sentence[room:]
            if current:
                pages.append(current)
                current = ""
            if len(sentence) > limit:
                pages.append(sentence[:limit])
                sentence = sentence[limit:]
        if len(current) + len(sentence) > limit and current:
            pages.append(current)
            current = ""
        current += sentence
    if current:
        pages.append(current)
    return pages


def split_balanced(text: str, count: int) -> list[str]:
    if count <= 0 or len(text) < count:
        raise ValueError("invalid comparison part count")
    boundaries = set("，。！？；：、”’")
    parts = []
    offset = 0
    for index in range(count - 1):
        remaining_parts = count - index
        remaining_chars = len(text) - offset
        ideal = offset + math.ceil(remaining_chars / remaining_parts)
        earliest = offset + max(
            1, remaining_chars - COMPARE_PART_CHAR_LIMIT * (remaining_parts - 1)
        )
        latest = min(
            offset + COMPARE_PART_CHAR_LIMIT,
            len(text) - (remaining_parts - 1),
        )
        candidates = [
            position for position in range(earliest, latest + 1)
            if text[position - 1] in boundaries
        ]
        end = min(candidates, key=lambda position: abs(position - ideal)) \
            if candidates else min(max(ideal, earliest), latest)
        parts.append(text[offset:end])
        offset = end
    parts.append(text[offset:])
    return parts


def load_plain_pages(path: Path) -> list[list[str]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    sections = payload.get("sections", [])
    if len(sections) != len(NOTES):
        raise SystemExit(f"expected {len(NOTES)} plain-text sections, found {len(sections)}")
    result = []
    for index, section in enumerate(sections, 1):
        pages = section.get("pages", [])
        if section.get("number") != index or not pages or any(not page.strip() for page in pages):
            raise SystemExit(f"invalid plain-text section {index}")
        if any(len(page) > PLAIN_PAGE_CHAR_LIMIT for page in pages):
            raise SystemExit(
                f"plain-text section {index} exceeds "
                f"{PLAIN_PAGE_CHAR_LIMIT} characters per page"
            )
        result.append(pages)
    return result


def build_data(url: str, plain_path: Path) -> dict:
    source_sections = fetch_sections(url)
    plain_sections = load_plain_pages(plain_path)
    sections = []
    for index, ((source_title, text), note) in enumerate(zip(source_sections, NOTES), 1):
        title, key, insight, practice = note
        if not source_title.startswith(title):
            raise SystemExit(f"section {index} mismatch: {source_title} != {title}")
        pages = paginate(text)
        plain_pages = plain_sections[index - 1]
        if len(plain_pages) != len(pages):
            raise SystemExit(
                f"section {index} page mismatch: {len(pages)} source, "
                f"{len(plain_pages)} plain"
            )
        sections.append({
            "number": index,
            "title": title,
            "text": text,
            "pages": pages,
            "plain_pages": plain_pages,
            "key": key,
            "highlight": exact_highlight(text, key),
            "cloze_easy": mask_text(key, 1),
            "cloze_hard": mask_text(key, 2),
            "insight": insight,
            "practice": practice,
        })
    return {
        "title": "金刚般若波罗蜜经",
        "translator": "后秦 鸠摩罗什译",
        "source": url,
        "notice": "解读与练习为本应用的学习提示，不替代经典注疏。",
        "sections": sections,
    }


def c_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def emit_c(data: dict) -> str:
    pages = []
    compare_source_parts = []
    compare_plain_parts = []
    compare_ranges = []
    section_rows = []
    for section in data["sections"]:
        start = len(pages)
        pages.extend(section["pages"])
        for source_page, plain_page in zip(section["pages"],
                                           section["plain_pages"]):
            count = max(
                math.ceil(len(source_page) / COMPARE_PART_CHAR_LIMIT),
                math.ceil(len(plain_page) / COMPARE_PART_CHAR_LIMIT),
            )
            source_parts = split_balanced(source_page, count)
            plain_parts = split_balanced(plain_page, count)
            compare_ranges.append((len(compare_source_parts), count))
            compare_source_parts.extend(source_parts)
            compare_plain_parts.extend(plain_parts)
        section_rows.append((section, start, len(section["pages"])))

    lines = [
        "/* Generated by tools/sutra_content.py. Do not edit by hand. */",
        '#include "sutra_content.h"',
        "",
        "static const char *const SUTRA_PAGES[] = {",
    ]
    lines.extend(f"    {c_string(page)}," for page in pages)
    lines.extend([
        "};",
        "",
        "typedef struct {",
        "    uint16_t start;",
        "    uint8_t count;",
        "} sutra_compare_range_t;",
        "",
        "static const sutra_compare_range_t SUTRA_COMPARE_RANGES[] = {",
    ])
    lines.extend(f"    {{ .start = {start}, .count = {count} }},"
                 for start, count in compare_ranges)
    lines.extend(["};", "", "static const char *const SUTRA_COMPARE_SOURCE[] = {"])
    lines.extend(f"    {c_string(page)}," for page in compare_source_parts)
    lines.extend(["};", "", "static const char *const SUTRA_COMPARE_PLAIN[] = {"])
    lines.extend(f"    {c_string(page)}," for page in compare_plain_parts)
    lines.extend(["};", "", "const sutra_section_t SUTRA_SECTIONS[SUTRA_SECTION_COUNT] = {"])
    for section, start, count in section_rows:
        lines.extend([
            "    {",
            f"        .number = {section['number']},",
            f"        .title = {c_string(section['title'])},",
            f"        .key = {c_string(section['key'])},",
            f"        .highlight = {c_string(section['highlight'])},",
            f"        .cloze_easy = {c_string(section['cloze_easy'])},",
            f"        .cloze_hard = {c_string(section['cloze_hard'])},",
            f"        .insight = {c_string(section['insight'])},",
            f"        .practice = {c_string(section['practice'])},",
            f"        .page_start = {start},",
            f"        .page_count = {count},",
            "    },",
        ])
    lines.extend([
        "};",
        "",
        "const char *sutra_text_page(size_t section_index, size_t page_index)",
        "{",
        "    if (section_index >= SUTRA_SECTION_COUNT) return NULL;",
        "    const sutra_section_t *section = &SUTRA_SECTIONS[section_index];",
        "    if (page_index >= section->page_count) return NULL;",
        "    return SUTRA_PAGES[section->page_start + page_index];",
        "}",
        "",
        "size_t sutra_compare_part_count(size_t section_index, size_t page_index)",
        "{",
        "    if (section_index >= SUTRA_SECTION_COUNT) return 0;",
        "    const sutra_section_t *section = &SUTRA_SECTIONS[section_index];",
        "    if (page_index >= section->page_count) return 0;",
        "    return SUTRA_COMPARE_RANGES[section->page_start + page_index].count;",
        "}",
        "",
        "const char *sutra_compare_source_part(size_t section_index,",
        "                                      size_t page_index,",
        "                                      size_t part_index)",
        "{",
        "    if (section_index >= SUTRA_SECTION_COUNT) return NULL;",
        "    const sutra_section_t *section = &SUTRA_SECTIONS[section_index];",
        "    if (page_index >= section->page_count) return NULL;",
        "    const sutra_compare_range_t *range =",
        "        &SUTRA_COMPARE_RANGES[section->page_start + page_index];",
        "    if (part_index >= range->count) return NULL;",
        "    return SUTRA_COMPARE_SOURCE[range->start + part_index];",
        "}",
        "",
        "const char *sutra_compare_plain_part(size_t section_index,",
        "                                     size_t page_index,",
        "                                     size_t part_index)",
        "{",
        "    if (section_index >= SUTRA_SECTION_COUNT) return NULL;",
        "    const sutra_section_t *section = &SUTRA_SECTIONS[section_index];",
        "    if (page_index >= section->page_count) return NULL;",
        "    const sutra_compare_range_t *range =",
        "        &SUTRA_COMPARE_RANGES[section->page_start + page_index];",
        "    if (part_index >= range->count) return NULL;",
        "    return SUTRA_COMPARE_PLAIN[range->start + part_index];",
        "}",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=SOURCE_URL)
    parser.add_argument("--json", default="main/sutra_data/diamond_sutra.json")
    parser.add_argument("--plain-json", default=PLAIN_PAGES_PATH)
    parser.add_argument("--c", default="main/sutra_content.c")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    json_path = Path(args.json)
    plain_path = Path(args.plain_json)
    c_path = Path(args.c)
    if args.check:
        data = json.loads(json_path.read_text(encoding="utf-8"))
        if len(data.get("sections", [])) != 32:
            raise SystemExit("content check failed: expected 32 sections")
        if any(not item.get("pages") or not item.get("key") or
               not item.get("highlight") or
               not item.get("plain_pages") or
               not item.get("insight") or not item.get("practice")
               for item in data["sections"]):
            raise SystemExit("content check failed: an application field is empty")
        if any(len(item["plain_pages"]) != len(item["pages"])
               for item in data["sections"]):
            raise SystemExit("content check failed: source/plain page counts differ")
        plain_sections = load_plain_pages(plain_path)
        if any(item["plain_pages"] != plain_sections[index]
               for index, item in enumerate(data["sections"])):
            raise SystemExit("content check failed: regenerate checked-in plain text")
        if any(not page.strip()
               for item in data["sections"] for page in item["plain_pages"]):
            raise SystemExit("content check failed: a plain-text page is empty")
        if any(not any(item["highlight"] in page for page in item["pages"])
               for item in data["sections"]):
            raise SystemExit("content check failed: a highlight crosses page boundaries")
        if c_path.read_text(encoding="utf-8") != emit_c(data):
            raise SystemExit("content check failed: regenerate main/sutra_content.c")
        print("Diamond Sutra content check: PASS")
        return 0

    data = build_data(args.url, plain_path)
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    c_path.write_text(emit_c(data), encoding="utf-8")
    print(f"generated {len(data['sections'])} sections at {json_path} and {c_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
