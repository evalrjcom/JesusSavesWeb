#!/usr/bin/env python3
# make_welcome.py v1.1.2 - generates netsurf/resources/<lang>/welcome.html for JesusSaves
# Usage: python3 make_welcome.py <netsurf source dir>
#
# One template, six languages (en de it ja nl zh_CN). Interface text is translated;
# Scripture is quoted from the public-domain King James Version in every language
# (the link targets are English-language sites). Search form: DuckDuckGo (HTML endpoint).
#
# Link policy (v1.1.2): sites that teach Jesus Christ as the only way of salvation
# (John 14:6, Acts 4:12): Bible reference sites, Billy Graham / evangelistic ministries,
# Baptist bodies, sound-doctrine teaching ministries. No prosperity-gospel sites.
import html
import os
import re
import sys

NS = sys.argv[1] if len(sys.argv) > 1 else "netsurf"

# ---- link sets (URL, label) - labels are proper names, not translated -----------------
BIBLE = [
    ("https://www.biblegateway.com/", "Bible Gateway"),
    ("https://www.blueletterbible.org/", "Blue Letter Bible"),
    ("https://biblehub.com/", "Bible Hub"),
    ("https://www.bible.com/", "YouVersion Bible"),
    ("https://www.esv.org/", "ESV Bible"),
]
EVANGELISTS = [
    ("https://billygraham.org/", "Billy Graham Evangelistic Association"),
    ("https://peacewithgod.net/", "Peace With God"),
    ("https://decisionmagazine.com/", "Decision Magazine"),
    ("https://www.livingwaters.com/", "Living Waters"),
    ("http://christpath.org", "ChristPath"),
]
BAPTIST = [
    ("https://www.sbc.net/", "Southern Baptist Convention"),
    ("https://www.baptistpress.com/", "Baptist Press"),
    ("https://www.lifeway.com/", "LifeWay"),
    ("https://www.imb.org/", "International Mission Board"),
    ("https://www.9marks.org/", "9Marks"),
    ("https://founders.org/", "Founders Ministries"),
]
TEACHING = [
    ("https://www.desiringgod.org/", "Desiring God"),
    ("https://www.ligonier.org/", "Ligonier Ministries"),
    ("https://www.gty.org/", "Grace to You"),
    ("https://www.thegospelcoalition.org/", "The Gospel Coalition"),
    ("https://www.gotquestions.org/", "GotQuestions.org"),
]

JOHN = "Jesus saith unto him, I am the way, the truth, and the life: no man cometh unto the Father, but by me."
ACTS = ("Neither is there salvation in any other: for there is none other name under heaven "
        "given among men, whereby we must be saved.")

# ---- per language interface text --------------------------------------------------------
L = {
    "en": dict(
        title="Welcome to JesusSaves", h="Welcome to JesusSaves", search="DuckDuckGo Search",
        intro=("JesusSaves is a small, fast web browser, a modified build of NetSurf. Changes were made by the "
               "JesusSaves project; NetSurf is the work of the NetSurf Developers. Thanks for choosing JesusSaves!"),
        nav=("Changelog", "Credits", "Licence"),
        verses="The only way", kjv="King James Version",
        cols=("Read the Bible", "Evangelism &amp; the Gospel", "Baptist", "Sound teaching"),
        foot=("JesusSaves is licensed under the GNU General Public Licence version 2, as is NetSurf, on which it is "
              "based. Modifications &copy; 2026 the JesusSaves project."),
        note="Every link here points to a ministry that holds to Christ alone for salvation. No prosperity gospel."),
    "de": dict(
        title="Willkommen bei JesusSaves", h="Willkommen bei JesusSaves", search="DuckDuckGo-Suche",
        intro=("JesusSaves ist ein kleiner, schneller Webbrowser, eine ver&auml;nderte Version von NetSurf. "
               "Die &Auml;nderungen stammen vom JesusSaves-Projekt; NetSurf ist das Werk der NetSurf-Entwickler. "
               "Danke, dass Sie JesusSaves gew&auml;hlt haben!"),
        nav=("&Auml;nderungsprotokoll", "Mitwirkende", "Lizenz"),
        verses="Der einzige Weg", kjv="King James Version (englisch)",
        cols=("Die Bibel lesen", "Evangelisation &amp; Evangelium", "Baptisten", "Gesunde Lehre"),
        foot=("JesusSaves steht unter der GNU General Public Licence Version 2, ebenso wie NetSurf, worauf es "
              "basiert. &Auml;nderungen &copy; 2026 das JesusSaves-Projekt."),
        note="Jeder Link f&uuml;hrt zu einem Dienst, der allein auf Christus f&uuml;r die Rettung vertraut. Kein Wohlstandsevangelium."),
    "it": dict(
        title="Benvenuti su JesusSaves", h="Benvenuti su JesusSaves", search="Ricerca DuckDuckGo",
        intro=("JesusSaves &egrave; un browser piccolo e veloce, una versione modificata di NetSurf. Le modifiche "
               "sono del progetto JesusSaves; NetSurf &egrave; opera degli sviluppatori di NetSurf. "
               "Grazie per aver scelto JesusSaves!"),
        nav=("Novit&agrave;", "Crediti", "Licenza"),
        verses="L'unica via", kjv="King James Version (inglese)",
        cols=("Leggere la Bibbia", "Evangelizzazione e Vangelo", "Battisti", "Dottrina sana"),
        foot=("JesusSaves &egrave; distribuito con la GNU General Public Licence versione 2, come NetSurf, su cui si "
              "basa. Modifiche &copy; 2026 il progetto JesusSaves."),
        note="Ogni collegamento porta a un ministero che si affida a Cristo solo per la salvezza. Nessun vangelo della prosperit&agrave;."),
    "ja": dict(
        title="JesusSavesへようこそ", h="JesusSavesへようこそ", search="DuckDuckGo検索",
        intro=("JesusSavesは小さくて速いウェブブラウザで、NetSurfを改変したものです。変更はJesusSavesプロジェクトによるものです。"
               "NetSurfはNetSurf開発者の成果です。JesusSavesをお選びいただきありがとうございます。"),
        nav=("変更履歴", "クレジット", "ライセンス"),
        verses="ただ一つの道", kjv="欽定訳聖書 (英語)",
        cols=("聖書を読む", "伝道と福音", "バプテスト", "健全な教え"),
        foot=("JesusSavesは、元になっているNetSurfと同じくGNU General Public Licenseバージョン2で提供されます。"
              "改変部分の著作権 &copy; 2026 JesusSavesプロジェクト。"),
        note="ここにあるリンクはすべて、救いをキリストのみに置く働きへのものです。繁栄の福音はありません。"),
    "nl": dict(
        title="Welkom bij JesusSaves", h="Welkom bij JesusSaves", search="DuckDuckGo zoeken",
        intro=("JesusSaves is een kleine, snelle webbrowser, een aangepaste versie van NetSurf. De wijzigingen zijn "
               "gemaakt door het JesusSaves-project; NetSurf is het werk van de NetSurf-ontwikkelaars. "
               "Bedankt dat je JesusSaves hebt gekozen!"),
        nav=("Wijzigingen", "Bijdragen", "Licentie"),
        verses="De enige weg", kjv="King James Version (Engels)",
        cols=("De Bijbel lezen", "Evangelisatie &amp; evangelie", "Baptisten", "Gezonde leer"),
        foot=("JesusSaves is uitgegeven onder de GNU General Public Licence versie 2, net als NetSurf, waarop het "
              "gebaseerd is. Wijzigingen &copy; 2026 het JesusSaves-project."),
        note="Elke link verwijst naar een bediening die voor redding op Christus alleen vertrouwt. Geen welvaartsevangelie."),
    "zh_CN": dict(
        title="欢迎使用 JesusSaves", h="欢迎使用 JesusSaves", search="DuckDuckGo 搜索",
        intro=("JesusSaves 是一款体积小、速度快的网络浏览器，基于 NetSurf 修改而成。修改由 JesusSaves 项目完成；"
               "NetSurf 是 NetSurf 开发人员的作品。感谢您选择 JesusSaves！"),
        nav=("更新日志", "制作人员", "许可协议"),
        verses="唯一的道路", kjv="英王钦定本 (英文)",
        cols=("阅读圣经", "传福音", "浸信会", "纯正教导"),
        foot=("JesusSaves 与其所基于的 NetSurf 一样，采用 GNU 通用公共许可证第 2 版发布。"
              "修改部分版权所有 &copy; 2026 JesusSaves 项目。"),
        note="这里的每个链接都指向仅凭基督得救的事工。没有成功神学（繁荣福音）。"),
}

# ---- the template CSS is taken from the existing English page so the look is unchanged --------
en_path = os.path.join(NS, "resources", "en", "welcome.html")
css = re.search(r'<style type="text/css">(.*?)</style>', open(en_path, encoding="utf-8").read(), re.S).group(1)
css = css.split(".verse{")[0]  # idempotent: drop rules appended by an earlier run
css += (".verse{width:80%;margin:1em auto;padding:0.6em 1em;border-left:4px solid #94adff;background:#f4f7ff;color:#222;}"
        ".verse b{display:block;margin-bottom:0.3em;}.verse i{color:#555;}"
        ".note{width:80%;margin:0 auto 1.5em;color:#555;font-size:90%;text-align:center;}"
        ".links ul{vertical-align:top;}.links h3{font-size:100%;margin:0 0 0.4em;}")


def page(lang, t):
    cols = [BIBLE, EVANGELISTS, BAPTIST, TEACHING]
    col_html = ""
    for i, c in enumerate(cols):
        col_html += "<ul>\n<li><b>%s</b></li>\n%s\n</ul>\n\n" % (
            t["cols"][i], "\n".join('<li><a href="%s">%s</a></li>' % (html.escape(u, quote=True), html.escape(n))
                                    for u, n in c))
    nav = t["nav"]
    return """<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<meta http-equiv="content-type" content="text/html; charset=utf-8">
<title>%(title)s</title>
<style type="text/css">%(css)s</style>
</head>

<body>
<h1 class="banner"><a href="about:welcome"><img src="about:logo" alt="JesusSaves"></a></h1>

<ul class="nslinks">
<li><a href="https://www.biblegateway.com/">Bible Gateway</a></li>
<li><a href="https://billygraham.org/">Billy Graham</a></li>
<li><a href="https://www.sbc.net/">Southern Baptist</a></li>
<li><a href="http://christpath.org">ChristPath</a></li>
<li><a href="about:changelog">%(n0)s</a></li>
<li><a href="about:credits">%(n1)s</a></li>
<li><a href="about:licence">%(n2)s</a></li>
</ul>

<div class="onlycontent">
<h2 class="version">%(h)s</h2>

<p class="intro">%(intro)s</p>

<div class="verse">
<b>%(verses)s <i>(%(kjv)s)</i></b>
<p>&ldquo;%(john)s&rdquo; &mdash; <a href="https://www.biblegateway.com/passage/?search=John+14%%3A6&amp;version=KJV">John 14:6</a></p>
<p>&ldquo;%(acts)s&rdquo; &mdash; <a href="https://www.biblegateway.com/passage/?search=Acts+4%%3A12&amp;version=KJV">Acts 4:12</a></p>
</div>

<form method="get" action="https://html.duckduckgo.com/html/">
<div class="websearch">
<input type="text" name="q" size="42" maxlength="255">
<input type="submit" value="%(search)s">
</div>
</form>

<div class="links">
%(cols)s</div>

<p class="note">%(note)s</p>

<div class="footer">
<p>%(foot)s</p>
</div>
</div>

</body>
</html>
""" % dict(title=t["title"], css=css, n0=nav[0], n1=nav[1], n2=nav[2], h=t["h"], intro=t["intro"],
           verses=t["verses"], kjv=t["kjv"], john=JOHN, acts=ACTS, search=t["search"], cols=col_html,
           note=t["note"], foot=t["foot"])


for lang, t in L.items():
    path = os.path.join(NS, "resources", lang, "welcome.html")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(page(lang, t))
    print("wrote", path)
