#!/bin/bash
# Proverbs 31 daily verse - day of month selects verse 1-31
# Shows both English (KJV) and Chinese (CUV, public domain)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# house root = 9 levels up (page_1/branches/<this> -> pages -> event_pkg -> reader -> pieces -> _.book-stack -> _.monads -> house); it was 7, which landed on _.book-stack,
# so the show-text binary was never found and the verse only went to stdout (no window).
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../../../.." && pwd)"
PACKAGE_DIR="${PACKAGE_DIR:-$HOUSE/_.monads/_.book-stack/entities/book-stack}"

# Proverbs 31:1-31 (KJV + CUV public domain)
VERSES_EN=(
  "The words of king Lemuel, the prophecy that his mother taught him."
  "What, my son? and what, the son of my womb? and what, the son of my vows?"
  "Give not thy strength unto women, nor thy ways to that which destroyeth kings."
  "It is not for kings, O Lemuel, it is not for kings to drink wine; nor for princes strong drink:"
  "Lest they drink, and forget the law, and pervert the judgment of any of the afflicted."
  "Give strong drink unto him that is ready to perish, and wine unto those that be of heavy hearts."
  "Let him drink, and forget his poverty, and remember his misery no more."
  "Open thy mouth for the dumb in the cause of all such as are appointed to destruction."
  "Open thy mouth, judge righteously, and plead the cause of the poor and needy."
  "Who can find a virtuous woman? for her price is far above rubies."
  "The heart of her husband doth safely trust in her, so that he shall have no need of spoil."
  "She will do him good and not evil all the days of her life."
  "She seeketh wool, and flax, and worketh willingly with her hands."
  "She is like the merchants' ships; she bringeth her food from afar."
  "She riseth also while it is yet night, and giveth meat to her household, and a portion to her maidens."
  "She considereth a field, and buyeth it: with the fruit of her hands she planteth a vineyard."
  "She girdeth her loins with strength, and strengtheneth her arms."
  "She perceiveth that her merchandise is good: her candle goeth not out by night."
  "She layeth her hands to the spindle, and her hands hold the distaff."
  "She stretcheth out her hand to the poor; yea, she reacheth forth her hands to the needy."
  "She is not afraid of the snow for her household: for all her household are clothed with scarlet."
  "She maketh herself coverings of tapestry; her clothing is silk and purple."
  "Her husband is known in the gates, when he sitteth among the elders of the land."
  "She maketh fine linen, and selleth it; and delivereth girdles unto the merchant."
  "Strength and honour are her clothing; and she shall rejoice in time to come."
  "She openeth her mouth with wisdom; and in her tongue is the law of kindness."
  "She looketh well to the ways of her household, and eateth not the bread of idleness."
  "Her children arise up, and call her blessed; her husband also, and he praiseth her."
  "Many daughters have done virtuously, but thou excellest them all."
  "Favour is deceitful, and beauty is vain: but a woman that feareth the LORD, she shall be praised."
  "Give her of the fruit of her hands; and let her own works praise her in the gates."
)

VERSES_ZH=(
  "這是雅基君王利慕伊勒的言語，是他母親教訓他的默示。"
  "我兒啊，我腹中生的兒啊，我許願得的兒啊，我當勸勉你甚麼呢？"
  "不要將你的力氣給婦女，也不要將你的行為給那傾覆君王的。"
  "利慕伊勒啊，君王飲酒、王子飲濃酒都不相宜；"
  "恐怕他們喝了就忘記律例，顛倒一切受苦人的案件。"
  "當給將死的人濃酒，給心中愁苦的人清酒。"
  "他喝了就忘記貧窮，也不再紀念苦楚。"
  "你當為啞巴開口，為一切孤獨的伸冤。"
  "你當開口按公義判斷，伸張困苦窮乏人的案件。"
  "誰能找到賢德的婦人呢？她的價值遠勝過珍珠。"
  "她丈夫心裡倚靠她，就不缺少利益。"
  "她一生使丈夫有益無損。"
  "她尋找羊毛、麻布，甘心用手做工。"
  "她好像商船，從遠處運糧食來。"
  "天還沒亮她就起來，給家中分派食物，使女分配工作。"
  "她想得田地，就買來；用手裡的收益栽種葡萄園。"
  "她以能力束腰，使膀臂強壯。"
  "她覺得經營有利；她的燈終夜不滅。"
  "她手拿籠頭，手把紡線。"
  "她張手賙濟貧窮，伸手幫補窮乏。"
  "她不因下雪為家中擔憂，因為全家都穿著朱紅衣服。"
  "她為自己製造繡花毯子；她的衣服是細麻布和紫色布料。"
  "她丈夫在城門口與國內的長老同坐，為人所認識。"
  "她做細麻布衣裳出賣，又將腰帶賣給商家。"
  "能力和威儀是她的衣服；她想到後來的日子就喜樂。"
  "她開口就發智慧；她舌頭上有仁慈的法則。"
  "她觀察家務，不吃閒飯。"
  "她兒女起來稱她有福；她丈夫也稱讚她，說："
  "「行事賢德的婦女很多，但你勝過一切。」"
  "恩寵是虛假的，美容是虛浮的；惟敬畏耶和華的婦人，必得稱讚。"
  "願她享受手裡的果子；願她的作品在城門口讚美她。"
)

DAY=$(date +%d)
DAY=$((10#$DAY))  # strip leading zero
if [ "$DAY" -lt 1 ] || [ "$DAY" -gt 31 ]; then DAY=1; fi

VERSE_EN="${VERSES_EN[$((DAY - 1))]}"
VERSE_ZH="${VERSES_ZH[$((DAY - 1))]}"

TMP=$(mktemp --suffix=.txt 2>/dev/null || echo /tmp/proverbs31_$$.txt)
# wrap by CHARACTER, not byte: byte-wise fold cut Chinese characters in half
{ printf "Proverbs 31:%s\n" "$DAY"; printf "%s\n" "$VERSE_EN" | fold -s -w 70; printf "\n箴言 31:%s\n" "$DAY"; printf "%s\n" "$VERSE_ZH" | LC_ALL=C.UTF-8 sed -E 's/(.{1,24})/\1\n/g' | sed '/^$/d'; } > "$TMP"

SHOW="$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x"
if [ -x "$SHOW" ]; then
    "$SHOW" "$PACKAGE_DIR" "$TMP"
else
    cat "$TMP"
fi