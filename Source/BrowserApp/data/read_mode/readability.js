var dbg = (typeof console !== 'undefined') ? function(s) {
    console.log("Readability: " + s);
} : function() {};
var readability = {
    version:                '1.7.1',
    emailSrc:               'http://lab.arc90.com/experiments/readability/email.php',
    iframeLoads:             0,
    convertLinksToFootnotes: false,
    reversePageScroll:       false, 
    frameHack:               false, 
    biggestFrame:            false,
    bodyCache:               null,   
    flags:                   0x1 | 0x2 | 0x4,   

    FLAG_STRIP_UNLIKELYS:     0x1,
    FLAG_WEIGHT_CLASSES:      0x2,
    FLAG_CLEAN_CONDITIONALLY: 0x4,

    maxPages:    30, 
    parsedPages: {}, 
    pageETags:   {}, 
    regexps: {
        unlikelyCandidates:    /combx|comment|community|disqus|extra|foot|header|menu|remark|rss|shoutbox|sidebar|sponsor|ad-break|agegate|pagination|pager|popup|tweet|twitter/i,
        okMaybeItsACandidate:  /and|article|body|column|main|shadow/i,
        positive:              /article|body|content|entry|hentry|main|page|pagination|post|text|blog|story/i,
        negative:              /combx|comment|com-|contact|foot|footer|footnote|masthead|media|meta|outbrain|promo|related|scroll|shoutbox|sidebar|sponsor|shopping|tags|tool|widget/i,
        extraneous:            /print|archive|comment|discuss|e[\-]?mail|share|reply|all|login|sign|single/i,
        divToPElements:        /<(a|blockquote|dl|div|img|ol|p|pre|table|ul)/i,
        replaceBrs:            /(<br[^>]*>[ \n\r\t]*){2,}/gi,
        replaceFonts:          /<(\/?)font[^>]*>/gi,
        trim:                  /^\s+|\s+$/g,
        normalize:             /\s{2,}/g,
        killBreaks:            /(<br\s*\/?>(\s|&nbsp;?)*){1,}/g,
        videos:                /http:\/\/(www\.)?(youtube|vimeo)\.com/i,
        skipFootnoteLink:      /^\s*(\[?[a-z0-9]{1,2}\]?|^|edit|citation needed)\s*$/i,
        nextLink:              /(next|weiter|continue|>([^\|]|$)|»([^\|]|$))/i,
        prevLink:              /(prev|earl|old|new|<|«)/i
    },
    init: function() {
        window.onload = window.onunload = function() {};
        readability.removeScripts(document);
        if(document.body && !readability.bodyCache) {
            readability.bodyCache = document.body.innerHTML;
        }
        readability.parsedPages[window.location.href.replace(/\/$/, '')] = true;
        var nextPageLink = readability.findNextPageLink(document.body);
        readability.prepDocument();
        var overlay        = document.createElement("DIV");
        var innerDiv       = document.createElement("DIV");
        var articleTools   = readability.getArticleTools();
        var articleTitle   = readability.getArticleTitle();
        var articleContent = readability.grabArticle();
        var articleFooter  = readability.getArticleFooter();
        if(!articleContent) {
            articleContent    = document.createElement("DIV");
            articleContent.id = "readability-content";
            articleContent.innerHTML = ["对不起，无法在该page中使用阅读模式。"].join('');
            nextPageLink = null;
        }
        overlay.id              = "readOverlay";
        innerDiv.id             = "readInner";
        document.body.className = readStyle;
        document.dir            = readability.getSuggestedDirection(articleTitle.innerHTML);
        if (readStyle === "style-athelas" || readStyle === "style-apertura"){
            overlay.className = readStyle + " rdbTypekit";
        }
        else {
            overlay.className = readStyle;
        }
        innerDiv.className    = readMargin + " " + readSize;

        if(typeof(readConvertLinksToFootnotes) !== 'undefined' && readConvertLinksToFootnotes === true) {
            readability.convertLinksToFootnotes = true;
        }
        innerDiv.appendChild( articleTitle   );
        innerDiv.appendChild( articleContent );
        innerDiv.appendChild( articleFooter  );
         overlay.appendChild( articleTools   );
         overlay.appendChild( innerDiv       );
        document.body.innerHTML = "";
        document.body.insertBefore(overlay, document.body.firstChild);
        document.body.removeAttribute('style');
        if(readability.frameHack)
        {
            var readOverlay = document.getElementById('readOverlay');
            readOverlay.style.height = '100%';
            readOverlay.style.overflow = 'auto';
        }
        if((window.location.protocol + "//" + window.location.host + "/") === window.location.href)
        {
            articleContent.style.display = "none";
            var rootWarning = document.createElement('p');
                rootWarning.id = "readability-warning";
                rootWarning.innerHTML = "对不起，请在有文章的page中使用阅读模式。";

            innerDiv.insertBefore( rootWarning, articleContent );
        }

        readability.postProcessContent(articleContent);
        window.scrollTo(0, 0);
        if (readStyle === "style-athelas" || readStyle === "style-apertura") {
            readability.useRdbTypekit();
        }
        if (nextPageLink) {
            window.setTimeout(function() {
                readability.appendNextPage(nextPageLink);
            }, 500);
        }
        document.onkeydown = function(e) {
            var code = (window.event) ? event.keyCode : e.keyCode;
            if (code === 16) {
                readability.reversePageScroll = true;
                return;
            }
            if (code === 32) {
                readability.curScrollStep = 0;
                var windowHeight = window.innerHeight ? window.innerHeight : (document.documentElement.clientHeight ? document.documentElement.clientHeight : document.body.clientHeight);

                if(readability.reversePageScroll) {
                    readability.scrollTo(readability.scrollTop(), readability.scrollTop() - (windowHeight - 50), 20, 10);
                }
                else {
                    readability.scrollTo(readability.scrollTop(), readability.scrollTop() + (windowHeight - 50), 20, 10);
                }
                return false;
            }
        };
        document.onkeyup = function(e) {
            var code = (window.event) ? event.keyCode : e.keyCode;
            if (code === 16) {
                readability.reversePageScroll = false;
                return;
            }
        };
    },
    postProcessContent: function(articleContent) {
        if(readability.convertLinksToFootnotes && !window.location.href.match(/wikipedia\.org/g)) {
            readability.addFootnotes(articleContent);
        }
        readability.fixImageFloats(articleContent);
    },
    fixImageFloats: function (articleContent) {
        var imageWidthThreshold = Math.min(articleContent.offsetWidth, 800) * 0.55,
            images              = articleContent.getElementsByTagName('img');
        for(var i=0, il = images.length; i < il; i+=1) {
            var image = images[i];
            if(image.offsetWidth > imageWidthThreshold) {
                image.className += " blockImage";
            }
        }
    },
    getArticleTools: function () {
        var articleTools = document.createElement("DIV");
        articleTools.id  = "readTools";
        articleTools.innerHTML =
            "<a href='#' onclick='return window.location.reload()' title='返回原始页面' id='reload-page'>返回原始页面</a><br />" +
            "<a href='#' onclick='javascript:window.print();' title='打印页面' id='print-page'>打印页面</a>";
        return articleTools;
    },
    getSuggestedDirection: function(text) {
        function sanitizeText() {
            return text.replace(/@\w+/, "");
        }
        function countMatches(match) {
            var matches = text.match(new RegExp(match, "g"));
            return matches !== null ? matches.length : 0;
        }
        function isRTL() {
            var count_heb =  countMatches("[\\u05B0-\\u05F4\\uFB1D-\\uFBF4]");
            var count_arb =  countMatches("[\\u060C-\\u06FE\\uFB50-\\uFEFC]");
            return  (count_heb + count_arb) * 100 / text.length > 20;
        }
        text  = sanitizeText(text);
        return isRTL() ? "rtl" : "ltr";
    },
    getArticleTitle: function () {
        var curTitle = "",
            origTitle = "";
        try {
            curTitle = origTitle = document.title;
            if(typeof curTitle !== "string") { 
                curTitle = origTitle = readability.getInnerText(document.getElementsByTagName('title')[0]);
            }
        }
        catch(e) {}
        if(curTitle.match(/ [\|\-] /))
        {
            curTitle = origTitle.replace(/(.*)[\|\-] .*/gi,'$1');
            if(curTitle.split(' ').length < 3) {
                curTitle = origTitle.replace(/[^\|\-]*[\|\-](.*)/gi,'$1');
            }
        }
        else if(curTitle.indexOf(': ') !== -1)
        {
            curTitle = origTitle.replace(/.*:(.*)/gi, '$1');
            if(curTitle.split(' ').length < 3) {
                curTitle = origTitle.replace(/[^:]*[:](.*)/gi,'$1');
            }
        }
        else if(curTitle.length > 150 || curTitle.length < 15)
        {
            var hOnes = document.getElementsByTagName('h1');
            if(hOnes.length === 1)
            {
                curTitle = readability.getInnerText(hOnes[0]);
            }
        }
        curTitle = curTitle.replace( readability.regexps.trim, "" );
        if(curTitle.split(' ').length <= 4) {
            curTitle = origTitle;
        }
        var articleTitle = document.createElement("H1");
        articleTitle.innerHTML = curTitle;
        return articleTitle;
    },
    getArticleFooter: function () {
        var articleFooter = document.createElement("DIV");
        articleFooter.id = "readFooter";
        articleFooter.innerHTML = [
        "<div id='rdb-footer-print'>Excerpted from <cite>" + document.title + "</cite><br />" + window.location.href + "</div>","</div>"].join('');
        return articleFooter;
    },
    prepDocument: function () {
        if(document.body === null)
        {
            var body = document.createElement("body");
            try {
                document.body = body;
            }
            catch(e) {
                document.documentElement.appendChild(body);
                dbg(e);
            }
        }
        document.body.id = "readabilityBody";
        var frames = document.getElementsByTagName('frame');
        if(frames.length > 0)
        {
            var bestFrame = null;
            var bestFrameSize = 0;
            var biggestFrameSize = 0; 
            for(var frameIndex = 0; frameIndex < frames.length; frameIndex+=1)
            {
                var frameSize = frames[frameIndex].offsetWidth + frames[frameIndex].offsetHeight;
                var canAccessFrame = false;
                try {
                    var frameBody = frames[frameIndex].contentWindow.document.body;
                    canAccessFrame = true;
                }
                catch(eFrames) {
                    dbg(eFrames);
                }
                if(frameSize > biggestFrameSize) {
                    biggestFrameSize         = frameSize;
                    readability.biggestFrame = frames[frameIndex];
                }
                if(canAccessFrame && frameSize > bestFrameSize)
                {
                    readability.frameHack = true;
                    bestFrame = frames[frameIndex];
                    bestFrameSize = frameSize;
                }
            }
            if(bestFrame)
            {
                var newBody = document.createElement('body');
                newBody.innerHTML = bestFrame.contentWindow.document.body.innerHTML;
                newBody.style.overflow = 'scroll';
                document.body = newBody;
                var frameset = document.getElementsByTagName('frameset')[0];
                if(frameset) {
                    frameset.parentNode.removeChild(frameset); }
            }
        }
        for (var k=0;k < document.styleSheets.length; k+=1) {
            if (document.styleSheets[k].href !== null && document.styleSheets[k].href.lastIndexOf("readability") === -1) {
                document.styleSheets[k].disabled = true;
            }
        }
        var styleTags = document.getElementsByTagName("style");
        for (var st=0;st < styleTags.length; st+=1) {
            styleTags[st].textContent = "";
        }
        document.body.innerHTML = document.body.innerHTML.replace(readability.regexps.replaceBrs, '</p><p>').replace(readability.regexps.replaceFonts, '<$1span>');
    },
    addFootnotes: function(articleContent) {
        var footnotesWrapper = document.getElementById('readability-footnotes'),
            articleFootnotes = document.getElementById('readability-footnotes-list');
        if(!footnotesWrapper) {
            footnotesWrapper               = document.createElement("DIV");
            footnotesWrapper.id            = 'readability-footnotes';
            footnotesWrapper.innerHTML     = '<h3>References</h3>';
            footnotesWrapper.style.display = 'none';
            articleFootnotes    = document.createElement('ol');
            articleFootnotes.id = 'readability-footnotes-list';
            footnotesWrapper.appendChild(articleFootnotes);
            var readFooter = document.getElementById('readFooter');
            if(readFooter) {
                readFooter.parentNode.insertBefore(footnotesWrapper, readFooter);
            }
        }
        var articleLinks = articleContent.getElementsByTagName('a');
        var linkCount    = articleFootnotes.getElementsByTagName('li').length;
        for (var i = 0; i < articleLinks.length; i+=1)
        {
            var articleLink  = articleLinks[i],
                footnoteLink = articleLink.cloneNode(true),
                refLink      = document.createElement('a'),
                footnote     = document.createElement('li'),
                linkDomain   = footnoteLink.host ? footnoteLink.host : document.location.host,
                linkText     = readability.getInnerText(articleLink);
            if(articleLink.className && articleLink.className.indexOf('readability-DoNotFootnote') !== -1 || linkText.match(readability.regexps.skipFootnoteLink)) {
                continue;
            }
            linkCount+=1;
            refLink.href      = '#readabilityFootnoteLink-' + linkCount;
            refLink.innerHTML = '<small><sup>[' + linkCount + ']</sup></small>';
            refLink.className = 'readability-DoNotFootnote';
            try { refLink.style.color = 'inherit'; } catch(e) {}
            if(articleLink.parentNode.lastChild === articleLink) {
                articleLink.parentNode.appendChild(refLink);
            } else {
                articleLink.parentNode.insertBefore(refLink, articleLink.nextSibling);
            }
            articleLink.name        = 'readabilityLink-' + linkCount;
            try { articleLink.style.color = 'inherit'; } catch(err) {}
            footnote.innerHTML      = "<small><sup><a href='#readabilityLink-" + linkCount + "' title='Jump to Link in Article'>^</a></sup></small> ";
            footnoteLink.innerHTML  = (footnoteLink.title ? footnoteLink.title : linkText);
            footnoteLink.name       = 'readabilityFootnoteLink-' + linkCount;
            footnote.appendChild(footnoteLink);
            footnote.innerHTML = footnote.innerHTML + "<small> (" + linkDomain + ")</small>";
            articleFootnotes.appendChild(footnote);
        }
        if(linkCount > 0) {
            footnotesWrapper.style.display = 'block';
        }
    },
    useRdbTypekit: function () {
        var rdbHead      = document.getElementsByTagName('head')[0];
        var rdbTKScript  = document.createElement('script');
        var rdbTKCode    = null;
        var rdbTKLink    = document.createElement('a');
            rdbTKLink.setAttribute('class','rdbTK-powered');
            rdbTKLink.setAttribute('title','Fonts by Typekit');
            rdbTKLink.innerHTML = "Fonts by <span class='rdbTK'>Typekit</span>";
        if (readStyle === "style-athelas") {
            rdbTKCode = "sxt6vzy";
            dbg("Using Athelas Theme");
            rdbTKLink.setAttribute('href','http://typekit.com/?utm_source=readability&utm_medium=affiliate&utm_campaign=athelas');
            rdbTKLink.setAttribute('id','rdb-athelas');
            document.getElementById("rdb-footer-right").appendChild(rdbTKLink);
        }
        if (readStyle === "style-apertura") {
            rdbTKCode = "bae8ybu";
            dbg("Using Inverse Theme");
            rdbTKLink.setAttribute('href','http://typekit.com/?utm_source=readability&utm_medium=affiliate&utm_campaign=inverse');
            rdbTKLink.setAttribute('id','rdb-inverse');
            document.getElementById("rdb-footer-right").appendChild(rdbTKLink);
        }
        rdbTKScript.setAttribute('type','text/javascript');
        rdbTKScript.setAttribute('src',"http://use.typekit.com/" + rdbTKCode + ".js");
        rdbTKScript.setAttribute('charset','UTF-8');
        rdbHead.appendChild(rdbTKScript);
        var typekitLoader = function() {
            dbg("Looking for Typekit.");
            if(typeof Typekit !== "undefined") {
                try {
                    dbg("Caught typekit");
                    Typekit.load();
                    clearInterval(window.typekitInterval);
                } catch(e) {
                    dbg("Typekit error: " + e);
                }
            }
        };

        window.typekitInterval = window.setInterval(typekitLoader, 100);
    },
    prepArticle: function (articleContent) {
        readability.cleanStyles(articleContent);
        readability.killBreaks(articleContent);
        readability.cleanConditionally(articleContent, "form");
        readability.clean(articleContent, "object");
        readability.clean(articleContent, "h1");
        if(articleContent.getElementsByTagName('h2').length === 1) {
            readability.clean(articleContent, "h2");
        }
        readability.clean(articleContent, "iframe");
        readability.cleanHeaders(articleContent);
        readability.cleanConditionally(articleContent, "table");
        readability.cleanConditionally(articleContent, "ul");
        readability.cleanConditionally(articleContent, "div");
        var articleParagraphs = articleContent.getElementsByTagName('p');
        for(var i = articleParagraphs.length-1; i >= 0; i-=1) {
            var imgCount    = articleParagraphs[i].getElementsByTagName('img').length;
            var embedCount  = articleParagraphs[i].getElementsByTagName('embed').length;
            var objectCount = articleParagraphs[i].getElementsByTagName('object').length;
            if(imgCount === 0 && embedCount === 0 && objectCount === 0 && readability.getInnerText(articleParagraphs[i], false) === '') {
                articleParagraphs[i].parentNode.removeChild(articleParagraphs[i]);
            }
        }
        try {
            articleContent.innerHTML = articleContent.innerHTML.replace(/<br[^>]*>\s*<p/gi, '<p');
        }
        catch (e) {
            dbg("Cleaning innerHTML of breaks failed. This is an IE strict-block-elements bug. Ignoring.: " + e);
        }
    },
    initializeNode: function (node) {
        node.readability = {"contentScore": 0};
        switch(node.tagName) {
            case 'DIV':
                node.readability.contentScore += 5;
                break;
            case 'PRE':
            case 'TD':
            case 'BLOCKQUOTE':
                node.readability.contentScore += 3;
                break;
            case 'ADDRESS':
            case 'OL':
            case 'UL':
            case 'DL':
            case 'DD':
            case 'DT':
            case 'LI':
            case 'FORM':
                node.readability.contentScore -= 3;
                break;
            case 'H1':
            case 'H2':
            case 'H3':
            case 'H4':
            case 'H5':
            case 'H6':
            case 'TH':
                node.readability.contentScore -= 5;
                break;
        }
        node.readability.contentScore += readability.getClassWeight(node);
    },
    grabArticle: function (page) {
        var stripUnlikelyCandidates = readability.flagIsActive(readability.FLAG_STRIP_UNLIKELYS),
            isPaging = (page !== null) ? true: false;
        page = page ? page : document.body;
        var pageCacheHtml = page.innerHTML;
        var allElements = page.getElementsByTagName('*');
        var node = null;
        var nodesToScore = [];
        for(var nodeIndex = 0; (node = allElements[nodeIndex]); nodeIndex+=1) {
            if (stripUnlikelyCandidates) {
                var unlikelyMatchString = node.className + node.id;
                if (
                    (
                        unlikelyMatchString.search(readability.regexps.unlikelyCandidates) !== -1 &&
                        unlikelyMatchString.search(readability.regexps.okMaybeItsACandidate) === -1 &&
                        node.tagName !== "BODY"
                    )
                )
                {
                    dbg("Removing unlikely candidate - " + unlikelyMatchString);
                    node.parentNode.removeChild(node);
                    nodeIndex-=1;
                    continue;
                }
            }
            if (node.tagName === "P" || node.tagName === "TD" || node.tagName === "PRE") {
                nodesToScore[nodesToScore.length] = node;
            }
            if (node.tagName === "DIV") {
                if (node.innerHTML.search(readability.regexps.divToPElements) === -1) {
                    var newNode = document.createElement('p');
                    try {
                        newNode.innerHTML = node.innerHTML;
                        node.parentNode.replaceChild(newNode, node);
                        nodeIndex-=1;

                        nodesToScore[nodesToScore.length] = node;
                    }
                    catch(e) {
                        dbg("Could not alter div to p, probably an IE restriction, reverting back to div.: " + e);
                    }
                }
                else
                {
                    for(var i = 0, il = node.childNodes.length; i < il; i+=1) {
                        var childNode = node.childNodes[i];
                        if(childNode.nodeType === 3) { 
                            var p = document.createElement('p');
                            p.innerHTML = childNode.nodeValue;
                            p.style.display = 'inline';
                            p.className = 'readability-styled';
                            childNode.parentNode.replaceChild(p, childNode);
                        }
                    }
                }
            }
        }
        var candidates = [];
        for (var pt=0; pt < nodesToScore.length; pt+=1) {
            var parentNode      = nodesToScore[pt].parentNode;
            var grandParentNode = parentNode ? parentNode.parentNode : null;
            var innerText       = readability.getInnerText(nodesToScore[pt]);
            if(!parentNode || typeof(parentNode.tagName) === 'undefined') {
                continue;
            }
            if(innerText.length < 25) {
                continue; }
            if(typeof parentNode.readability === 'undefined') {
                readability.initializeNode(parentNode);
                candidates.push(parentNode);
            }
            if(grandParentNode && typeof(grandParentNode.readability) === 'undefined' && typeof(grandParentNode.tagName) !== 'undefined') {
                readability.initializeNode(grandParentNode);
                candidates.push(grandParentNode);
            }
            var contentScore = 0;
            contentScore+=1;
            contentScore += innerText.split(',').length;
            contentScore += Math.min(Math.floor(innerText.length / 100), 3);
            parentNode.readability.contentScore += contentScore;
            if(grandParentNode) {
                grandParentNode.readability.contentScore += contentScore/2;
            }
        }
        var topCandidate = null;
        for(var c=0, cl=candidates.length; c < cl; c+=1)
        {
            candidates[c].readability.contentScore = candidates[c].readability.contentScore * (1-readability.getLinkDensity(candidates[c]));
            dbg('Candidate: ' + candidates[c] + " (" + candidates[c].className + ":" + candidates[c].id + ") with score " + candidates[c].readability.contentScore);
            if(!topCandidate || candidates[c].readability.contentScore > topCandidate.readability.contentScore) {
                topCandidate = candidates[c]; }
        }
        if (topCandidate === null || topCandidate.tagName === "BODY")
        {
            topCandidate = document.createElement("DIV");
            topCandidate.innerHTML = page.innerHTML;
            page.innerHTML = "";
            page.appendChild(topCandidate);
            readability.initializeNode(topCandidate);
        }
        var articleContent        = document.createElement("DIV");
        if (isPaging) {
            articleContent.id     = "readability-content";
        }
        var siblingScoreThreshold = Math.max(10, topCandidate.readability.contentScore * 0.2);
        var siblingNodes          = topCandidate.parentNode.childNodes;
        for(var s=0, sl=siblingNodes.length; s < sl; s+=1) {
            var siblingNode = siblingNodes[s];
            var append      = false;
            if(!siblingNode) {
                continue;
            }
            dbg("Looking at sibling node: " + siblingNode + " (" + siblingNode.className + ":" + siblingNode.id + ")" + ((typeof siblingNode.readability !== 'undefined') ? (" with score " + siblingNode.readability.contentScore) : ''));
            dbg("Sibling has score " + (siblingNode.readability ? siblingNode.readability.contentScore : 'Unknown'));
            if(siblingNode === topCandidate)
            {
                append = true;
            }
            var contentBonus = 0;
            if(siblingNode.className === topCandidate.className && topCandidate.className !== "") {
                contentBonus += topCandidate.readability.contentScore * 0.2;
            }
            if(typeof siblingNode.readability !== 'undefined' && (siblingNode.readability.contentScore+contentBonus) >= siblingScoreThreshold)
            {
                append = true;
            }
            if(siblingNode.nodeName === "P") {
                var linkDensity = readability.getLinkDensity(siblingNode);
                var nodeContent = readability.getInnerText(siblingNode);
                var nodeLength  = nodeContent.length;
                if(nodeLength > 80 && linkDensity < 0.25)
                {
                    append = true;
                }
                else if(nodeLength < 80 && linkDensity === 0 && nodeContent.search(/\.( |$)/) !== -1)
                {
                    append = true;
                }
            }
            if(append) {
                dbg("Appending node: " + siblingNode);
                var nodeToAppend = null;
                if(siblingNode.nodeName !== "DIV" && siblingNode.nodeName !== "P") {
                    dbg("Altering siblingNode of " + siblingNode.nodeName + ' to div.');
                    nodeToAppend = document.createElement("DIV");
                    try {
                        nodeToAppend.id = siblingNode.id;
                        nodeToAppend.innerHTML = siblingNode.innerHTML;
                    }
                    catch(er) {
                        dbg("Could not alter siblingNode to div, probably an IE restriction, reverting back to original.");
                        nodeToAppend = siblingNode;
                        s-=1;
                        sl-=1;
                    }
                } else {
                    nodeToAppend = siblingNode;
                    s-=1;
                    sl-=1;
                }
                nodeToAppend.className = "";
                articleContent.appendChild(nodeToAppend);
            }
        }
        readability.prepArticle(articleContent);
        if (readability.curPageNum === 1) {
            articleContent.innerHTML = '<div id="readability-page-1" class="page">' + articleContent.innerHTML + '</div>';
        }
        if(readability.getInnerText(articleContent, false).length < 250) {
        page.innerHTML = pageCacheHtml;
            if (readability.flagIsActive(readability.FLAG_STRIP_UNLIKELYS)) {
                readability.removeFlag(readability.FLAG_STRIP_UNLIKELYS);
                return readability.grabArticle(page);
            }
            else if (readability.flagIsActive(readability.FLAG_WEIGHT_CLASSES)) {
                readability.removeFlag(readability.FLAG_WEIGHT_CLASSES);
                return readability.grabArticle(page);
            }
            else if (readability.flagIsActive(readability.FLAG_CLEAN_CONDITIONALLY)) {
                readability.removeFlag(readability.FLAG_CLEAN_CONDITIONALLY);
                return readability.grabArticle(page);
            } else {
                return null;
            }
        }
        return articleContent;
    },
    removeScripts: function (doc) {
        var scripts = doc.getElementsByTagName('script');
        for(var i = scripts.length-1; i >= 0; i-=1)
        {
            if(typeof(scripts[i].src) === "undefined" || (scripts[i].src.indexOf('readability') === -1 && scripts[i].src.indexOf('typekit') === -1))
            {
                scripts[i].nodeValue="";
                scripts[i].removeAttribute('src');
                if (scripts[i].parentNode) {
                        scripts[i].parentNode.removeChild(scripts[i]);
                }
            }
        }
    },
    getInnerText: function (e, normalizeSpaces) {
        var textContent    = "";
        if(typeof(e.textContent) === "undefined" && typeof(e.innerText) === "undefined") {
            return "";
        }
        normalizeSpaces = (typeof normalizeSpaces === 'undefined') ? true : normalizeSpaces;
        if (navigator.appName === "Microsoft Internet Explorer") {
            textContent = e.innerText.replace( readability.regexps.trim, "" ); }
        else {
            textContent = e.textContent.replace( readability.regexps.trim, "" ); }
        if(normalizeSpaces) {
            return textContent.replace( readability.regexps.normalize, " "); }
        else {
            return textContent; }
    },
    getCharCount: function (e,s) {
        s = s || ",";
        return readability.getInnerText(e).split(s).length-1;
    },
    cleanStyles: function (e) {
        e = e || document;
        var cur = e.firstChild;
        if(!e) {
            return; }
        if(typeof e.removeAttribute === 'function' && e.className !== 'readability-styled') {
            e.removeAttribute('style'); }
        while ( cur !== null ) {
            if ( cur.nodeType === 1 ) {
                if(cur.className !== "readability-styled") {
                    cur.removeAttribute("style");
                }
                readability.cleanStyles( cur );
            }
            cur = cur.nextSibling;
        }
    },
getLinkDensity: function (e) {
        var links      = e.getElementsByTagName("a");
        var textLength = readability.getInnerText(e).length;
        var linkLength = 0;
        for(var i=0, il=links.length; i<il;i+=1)
        {
            linkLength += readability.getInnerText(links[i]).length;
        }
        return linkLength / textLength;
    },
    findBaseUrl: function () {
        var noUrlParams     = window.location.pathname.split("?")[0],
            urlSlashes      = noUrlParams.split("/").reverse(),
            cleanedSegments = [],
            possibleType    = "";
        for (var i = 0, slashLen = urlSlashes.length; i < slashLen; i+=1) {
            var segment = urlSlashes[i];
            if (segment.indexOf(".") !== -1) {
                possibleType = segment.split(".")[1];
                if(!possibleType.match(/[^a-zA-Z]/)) {
                    segment = segment.split(".")[0];
                }
            }
            if(segment.indexOf(',00') !== -1) {
                segment = segment.replace(',00', '');
            }
            if (segment.match(/((_|-)?p[a-z]*|(_|-))[0-9]{1,2}$/i) && ((i === 1) || (i === 0))) {
                segment = segment.replace(/((_|-)?p[a-z]*|(_|-))[0-9]{1,2}$/i, "");
            }
            var del = false;
            if (i < 2 && segment.match(/^\d{1,2}$/)) {
                del = true;
            }
            if(i === 0 && segment.toLowerCase() === "index") {
                del = true;
            }
            if(i < 2 && segment.length < 3 && !urlSlashes[0].match(/[a-z]/i)) {
                del = true;
            }
            if (!del) {
                cleanedSegments.push(segment);
            }
        }
        return window.location.protocol + "//" + window.location.host + cleanedSegments.reverse().join("/");
    },
    findNextPageLink: function (elem) {
        var possiblePages = {},
            allLinks = elem.getElementsByTagName('a'),
            articleBaseUrl = readability.findBaseUrl();
        for(var i = 0, il = allLinks.length; i < il; i+=1) {
            var link     = allLinks[i],
                linkHref = allLinks[i].href.replace(/#.*$/, '').replace(/\/$/, '');
            if(linkHref === "" || linkHref === articleBaseUrl || linkHref === window.location.href || linkHref in readability.parsedPages) {
                continue;
            }
            if(window.location.host !== linkHref.split(/\/+/g)[1]) {
                continue;
            }

            var linkText = readability.getInnerText(link);
            if(linkText.match(readability.regexps.extraneous) || linkText.length > 25) {
                continue;
            }
            var linkHrefLeftover = linkHref.replace(articleBaseUrl, '');
            if(!linkHrefLeftover.match(/\d/)) {
                continue;
            }
            if(!(linkHref in possiblePages)) {
                possiblePages[linkHref] = {"score": 0, "linkText": linkText, "href": linkHref};
            } else {
                possiblePages[linkHref].linkText += ' | ' + linkText;
            }
            var linkObj = possiblePages[linkHref];
            if(linkHref.indexOf(articleBaseUrl) !== 0) {
                linkObj.score -= 25;
            }
            var linkData = linkText + ' ' + link.className + ' ' + link.id;
            if(linkData.match(readability.regexps.nextLink)) {
                linkObj.score += 50;
            }
            if(linkData.match(/pag(e|ing|inat)/i)) {
                linkObj.score += 25;
            }
            if(linkData.match(/(first|last)/i)) { 
                if(!linkObj.linkText.match(readability.regexps.nextLink)) {
                    linkObj.score -= 65;
                }
            }
            if(linkData.match(readability.regexps.negative) || linkData.match(readability.regexps.extraneous)) {
                linkObj.score -= 50;
            }
            if(linkData.match(readability.regexps.prevLink)) {
                linkObj.score -= 200;
            }
            var parentNode = link.parentNode,
                positiveNodeMatch = false,
                negativeNodeMatch = false;
            while(parentNode) {
                var parentNodeClassAndId = parentNode.className + ' ' + parentNode.id;
                if(!positiveNodeMatch && parentNodeClassAndId && parentNodeClassAndId.match(/pag(e|ing|inat)/i)) {
                    positiveNodeMatch = true;
                    linkObj.score += 25;
                }
                if(!negativeNodeMatch && parentNodeClassAndId && parentNodeClassAndId.match(readability.regexps.negative)) {
                    if(!parentNodeClassAndId.match(readability.regexps.positive)) {
                        linkObj.score -= 25;
                        negativeNodeMatch = true;
                    }
                }
                parentNode = parentNode.parentNode;
            }
            if (linkHref.match(/p(a|g|ag)?(e|ing|ination)?(=|\/)[0-9]{1,2}/i) || linkHref.match(/(page|paging)/i)) {
                linkObj.score += 25;
            }
            if (linkHref.match(readability.regexps.extraneous)) {
                linkObj.score -= 15;
            }
            var linkTextAsNumber = parseInt(linkText, 10);
            if(linkTextAsNumber) {
                if (linkTextAsNumber === 1) {
                    linkObj.score -= 10;
                }
                else {
                    linkObj.score += Math.max(0, 10 - linkTextAsNumber);
                }
            }
        }
        var topPage = null;
        for(var page in possiblePages) {
            if(possiblePages.hasOwnProperty(page)) {
                if(possiblePages[page].score >= 50 && (!topPage || topPage.score < possiblePages[page].score)) {
                    topPage = possiblePages[page];
                }
            }
        }
        if(topPage) {
            var nextHref = topPage.href.replace(/\/$/,'');
            dbg('NEXT PAGE IS ' + nextHref);
            readability.parsedPages[nextHref] = true;
            return nextHref;
        }
        else {
            return null;
        }
    },
    xhr: function () {
        if (typeof XMLHttpRequest !== 'undefined' && (window.location.protocol !== 'file:' || !window.ActiveXObject)) {
            return new XMLHttpRequest();
        }
        else {
            try { return new ActiveXObject('Msxml2.XMLHTTP.6.0'); } catch(sixerr) { }
            try { return new ActiveXObject('Msxml2.XMLHTTP.3.0'); } catch(threrr) { }
            try { return new ActiveXObject('Msxml2.XMLHTTP'); } catch(err) { }
        }
        return false;
    },
    successfulRequest: function (request) {
        return (request.status >= 200 && request.status < 300) || request.status === 304 || (request.status === 0 && request.responseText);
    },
    ajax: function (url, options) {
        var request = readability.xhr();
        function respondToReadyState(readyState) {
            if (request.readyState === 4) {
                if (readability.successfulRequest(request)) {
                    if (options.success) { options.success(request); }
                }
                else {
                    if (options.error) { options.error(request); }
                }
            }
        }
        if (typeof options === 'undefined') { options = {}; }
        request.onreadystatechange = respondToReadyState;
        request.open('get', url, true);
        request.setRequestHeader('Accept', 'text/html');
        try {
            request.send(options.postBody);
        }
        catch (e) {
            if (options.error) { options.error(); }
        }
        return request;
    },
    curPageNum: 1,
    appendNextPage: function (nextPageLink) {
        readability.curPageNum+=1;
        var articlePage       = document.createElement("DIV");
        articlePage.id        = 'readability-page-' + readability.curPageNum;
        articlePage.className = 'page';
        articlePage.innerHTML = '<p class="page-separator" title="Page ' + readability.curPageNum + '">&sect;</p>';
        document.getElementById("readability-content").appendChild(articlePage);
        if(readability.curPageNum > readability.maxPages) {
            var nextPageMarkup = "<div style='text-align: center'><a href='" + nextPageLink + "'>View Next Page</a></div>";
            articlePage.innerHTML = articlePage.innerHTML + nextPageMarkup;
            return;
        }
        (function(pageUrl, thisPage) {
            readability.ajax(pageUrl, {
                success: function(r) {
                    var eTag = r.getResponseHeader('ETag');
                    if(eTag) {
                        if(eTag in readability.pageETags) {
                            dbg("Exact duplicate page found via ETag. Aborting.");
                            articlePage.style.display = 'none';
                            return;
                        } else {
                            readability.pageETags[eTag] = 1;
                        }
                    }
                    var page = document.createElement("DIV");
                    var responseHtml = r.responseText.replace(/\n/g,'\uffff').replace(/<script.*?>.*?<\/script>/gi, '');
                    responseHtml = responseHtml.replace(/\n/g,'\uffff').replace(/<script.*?>.*?<\/script>/gi, '');
                    responseHtml = responseHtml.replace(/\uffff/g,'\n').replace(/<(\/?)noscript/gi, '<$1div');
                    responseHtml = responseHtml.replace(readability.regexps.replaceBrs, '</p><p>');
                    responseHtml = responseHtml.replace(readability.regexps.replaceFonts, '<$1span>');
                    page.innerHTML = responseHtml;
                    readability.flags = 0x1 | 0x2 | 0x4;
                    var nextPageLink = readability.findNextPageLink(page),
                        content      =  readability.grabArticle(page);
                    if(!content) {
                        dbg("No content found in page to append. Aborting.");
                        return;
                    }
                    var firstP = content.getElementsByTagName("P").length ? content.getElementsByTagName("P")[0] : null;
                    if(firstP && firstP.innerHTML.length > 100) {
                        for(var i=1; i <= readability.curPageNum; i+=1) {
                            var rPage = document.getElementById('readability-page-' + i);
                            if(rPage && rPage.innerHTML.indexOf(firstP.innerHTML) !== -1) {
                                dbg('Duplicate of page ' + i + ' - skipping.');
                                articlePage.style.display = 'none';
                                readability.parsedPages[pageUrl] = true;
                                return;
                            }
                        }
                    }
                    readability.removeScripts(content);
                    thisPage.innerHTML = thisPage.innerHTML + content.innerHTML;
                    window.setTimeout(
                        function() { readability.postProcessContent(thisPage); },
                        500
                    );
                    if(nextPageLink) {
                        readability.appendNextPage(nextPageLink);
                    }
                }
            });
        }(nextPageLink, articlePage));
    },
    getClassWeight: function (e) {
        if(!readability.flagIsActive(readability.FLAG_WEIGHT_CLASSES)) {
            return 0;
        }
        var weight = 0;
        if (typeof(e.className) === 'string' && e.className !== '')
        {
            if(e.className.search(readability.regexps.negative) !== -1) {
                weight -= 25; }
            if(e.className.search(readability.regexps.positive) !== -1) {
                weight += 25; }
        }
        if (typeof(e.id) === 'string' && e.id !== '')
        {
            if(e.id.search(readability.regexps.negative) !== -1) {
                weight -= 25; }

            if(e.id.search(readability.regexps.positive) !== -1) {
                weight += 25; }
        }
        return weight;
    },

    nodeIsVisible: function (node) {
        return (node.offsetWidth !== 0 || node.offsetHeight !== 0) && node.style.display.toLowerCase() !== 'none';
    },
    killBreaks: function (e) {
        try {
            e.innerHTML = e.innerHTML.replace(readability.regexps.killBreaks,'<br />');
        }
        catch (eBreaks) {
            dbg("KillBreaks failed - this is an IE bug. Ignoring.: " + eBreaks);
        }
    },
    clean: function (e, tag) {
        var targetList = e.getElementsByTagName( tag );
        var isEmbed    = (tag === 'object' || tag === 'embed');

        for (var y=targetList.length-1; y >= 0; y-=1) {
            if(isEmbed) {
                var attributeValues = "";
                for (var i=0, il=targetList[y].attributes.length; i < il; i+=1) {
                    attributeValues += targetList[y].attributes[i].value + '|';
                }
                if (attributeValues.search(readability.regexps.videos) !== -1) {
                    continue;
                }
                if (targetList[y].innerHTML.search(readability.regexps.videos) !== -1) {
                    continue;
                }
            }
            targetList[y].parentNode.removeChild(targetList[y]);
        }
    },
    cleanConditionally: function (e, tag) {
        if(!readability.flagIsActive(readability.FLAG_CLEAN_CONDITIONALLY)) {
            return;
        }
        var tagsList      = e.getElementsByTagName(tag);
        var curTagsLength = tagsList.length;
        for (var i=curTagsLength-1; i >= 0; i-=1) {
            var weight = readability.getClassWeight(tagsList[i]);
            var contentScore = (typeof tagsList[i].readability !== 'undefined') ? tagsList[i].readability.contentScore : 0;
            dbg("Cleaning Conditionally " + tagsList[i] + " (" + tagsList[i].className + ":" + tagsList[i].id + ")" + ((typeof tagsList[i].readability !== 'undefined') ? (" with score " + tagsList[i].readability.contentScore) : ''));
            if(weight+contentScore < 0)
            {
                tagsList[i].parentNode.removeChild(tagsList[i]);
            }
            else if ( readability.getCharCount(tagsList[i],',') < 10) {
                var p      = tagsList[i].getElementsByTagName("p").length;
                var img    = tagsList[i].getElementsByTagName("img").length;
                var li     = tagsList[i].getElementsByTagName("li").length-100;
                var input  = tagsList[i].getElementsByTagName("input").length;
                var embedCount = 0;
                var embeds     = tagsList[i].getElementsByTagName("embed");
                for(var ei=0,il=embeds.length; ei < il; ei+=1) {
                    if (embeds[ei].src.search(readability.regexps.videos) === -1) {
                      embedCount+=1;
                    }
                }
                var linkDensity   = readability.getLinkDensity(tagsList[i]);
                var contentLength = readability.getInnerText(tagsList[i]).length;
                var toRemove      = false;
                if ( img > p ) {
                    toRemove = true;
                } else if(li > p && tag !== "ul" && tag !== "ol") {
                    toRemove = true;
                } else if( input > Math.floor(p/3) ) {
                    toRemove = true;
                } else if(contentLength < 25 && (img === 0 || img > 2) ) {
                    toRemove = true;
                } else if(weight < 25 && linkDensity > 0.2) {
                    toRemove = true;
                } else if(weight >= 25 && linkDensity > 0.5) {
                    toRemove = true;
                } else if((embedCount === 1 && contentLength < 75) || embedCount > 1) {
                    toRemove = true;
                }
                if(toRemove) {
                    tagsList[i].parentNode.removeChild(tagsList[i]);
                }
            }
        }
    },
    cleanHeaders: function (e) {
        for (var headerIndex = 1; headerIndex < 3; headerIndex+=1) {
            var headers = e.getElementsByTagName('h' + headerIndex);
            for (var i=headers.length-1; i >=0; i-=1) {
                if (readability.getClassWeight(headers[i]) < 0 || readability.getLinkDensity(headers[i]) > 0.33) {
                    headers[i].parentNode.removeChild(headers[i]);
                }
            }
        }
    },
    easeInOut: function(start,end,totalSteps,actualStep) {
        var delta = end - start;
        if ((actualStep/=totalSteps/2) < 1) {
            return delta/2*actualStep*actualStep + start;
        }
        actualStep -=1;
        return -delta/2 * ((actualStep)*(actualStep-2) - 1) + start;
    },
    scrollTop: function(scroll){
        var setScroll = typeof scroll !== 'undefined';
        if(setScroll) {
            return window.scrollTo(0, scroll);
        }
        if(typeof window.pageYOffset !== 'undefined') {
            return window.pageYOffset;
        }
        else if(document.documentElement.clientHeight) {
            return document.documentElement.scrollTop;
        }
        else {
            return document.body.scrollTop;
        }
    },
    curScrollStep: 0,
    scrollTo: function (scrollStart, scrollEnd, steps, interval) {
        if(
            (scrollStart < scrollEnd && readability.scrollTop() < scrollEnd) ||
            (scrollStart > scrollEnd && readability.scrollTop() > scrollEnd)
          ) {
            readability.curScrollStep+=1;
            if(readability.curScrollStep > steps) {
                return;
            }
            var oldScrollTop = readability.scrollTop();
            readability.scrollTop(readability.easeInOut(scrollStart, scrollEnd, steps, readability.curScrollStep));
            if(oldScrollTop === readability.scrollTop()) {
                return;
            }
            window.setTimeout(function() {
                readability.scrollTo(scrollStart, scrollEnd, steps, interval);
            }, interval);
        }
    },
    emailBox: function () {
        var emailContainerExists = document.getElementById('email-container');
        if(null !== emailContainerExists)
        {
            return;
        }
        var emailContainer = document.createElement("DIV");
        emailContainer.setAttribute('id', 'email-container');
        emailContainer.innerHTML = '<iframe src="'+readability.emailSrc + '?pageUrl='+encodeURIComponent(window.location)+'&pageTitle='+encodeURIComponent(document.title)+'" scrolling="no" onload="readability.removeFrame()" style="width:500px; height: 490px; border: 0;"></iframe>';
        document.body.appendChild(emailContainer);
    },
    removeFrame: function () {
        readability.iframeLoads+=1;
        if (readability.iframeLoads > 3)
        {
            var emailContainer = document.getElementById('email-container');
            if (null !== emailContainer) {
                emailContainer.parentNode.removeChild(emailContainer);
            }

            readability.iframeLoads = 0;
        }
    },
    htmlspecialchars: function (s) {
        if (typeof(s) === "string") {
            s = s.replace(/&/g, "&amp;");
            s = s.replace(/"/g, "&quot;");
            s = s.replace(/'/g, "&#039;");
            s = s.replace(/</g, "&lt;");
            s = s.replace(/>/g, "&gt;");
        }
        return s;
    },
    flagIsActive: function(flag) {
        return (readability.flags & flag) > 0;
    },
    addFlag: function(flag) {
        readability.flags = readability.flags | flag;
    },
    removeFlag: function(flag) {
        readability.flags = readability.flags & ~flag;
    }
};
function main() {
	readability.init();
   _readability_css=document.createElement('style');
	_readability_css.rel='stylesheet';
//readability.css
	_readability_css.text="@charset \"utf-8\";body{font-size:10px;line-height:1;}#readability-logo,#arc90-logo,.footer-twitterLink,#readTools a,a.rdbTK-powered span{background-color:transparent!important;background-image:url(http://lab.arc90.com/experiments/readability/images/sprite-readability.png)!important;background-repeat:no-repeat!important;}#readOverlay{text-rendering:optimizeLegibility;display:block;position:absolute;top:0;left:0;width:100%;}#readInner{max-width:800px;margin:1em auto;}#readInner a{color:#039;text-decoration:none;}#readInner a:hover,#readInner a:focus{text-decoration:underline;}#readInner img{float:left;clear:both;margin:0 12px 12px 0;}#readInner img.blockImage{clear:both;float:none;display:block;margin-left:auto;margin-right:auto;}#readInner h1{display:block;width:100%;border-bottom:1px solid #333;font-size:1.2em;padding-bottom:.5em;margin-bottom:.75em;}#readInner sup{line-height:.8em;}#readInner .page-separator{clear:both;display:block;font-size:.85em;filter:alpha(opacity=20);opacity:.20;text-align:center;}.style-apertura #readInner h1{border-bottom-color:#ededed;}.style-terminal #readInner h1{border-bottom-color:#c6ffc6;}#readInner blockquote{margin-left:3em;margin-right:3em;}#readability-inner *{margin-bottom:16px;border:none;background:none;}#readability-footnotes{clear:both;}#rdb-footer-wrapper{display:block;border-top:1px solid #333;clear:both;overflow:hidden;margin:2em 0;}.style-apertura #rdb-footer-wrapper{border-top-color:#ededed;}.style-terminal #rdb-footer-wrapper{border-top-color:#c6ffc6;}#rdb-footer-left{display:inline;float:left;margin-top:15px;width:285px;background-position:0 -36px;}.rdbTypekit #rdb-footer-left{width:475px;}#rdb-footer-left a,#rdb-footer-left a:link{float:left;}#readability-logo{display:inline;background-position:0 -36px;height:29px;width:189px;text-indent:-9000px;}#arc90-logo{display:inline;background-position:right -36px;height:29px;width:96px;text-indent:-9000px;}.style-apertura #readability-logo,.style-terminal #readability-logo{background-position:0 -67px;}.style-apertura #arc90-logo,.style-terminal #arc90-logo{background-position:right -67px;}#rdb-footer-right{display:inline;float:right;text-align:right;font-size:.75em;margin-top:18px;}#rdb-footer-right a{display:inline-block;float:left;overflow:visible;line-height:16px;vertical-align:baseline;}.footer-twitterLink{height:20px;margin-left:20px;background-position:0 -123px;font-size:12px;padding:4px 0 0 28px;}#rdb-footer-left .footer-twitterLink{display:none;margin-top:1px;padding-top:2px;}.rdbTypekit #rdb-footer-left .footer-twitterLink{display:inline-block!important;}a.rdbTK-powered,a.rdbTK-powered:link,a.rdbTK-powered:hover{font-size:16px;color:#858789!important;text-decoration:none!important;}a.rdbTK-powered span{display:inline-block;height:22px;margin-left:2px;background-position:0 -146px!important;padding:4px 0 0 26px;}.style-apertura #rdb-inverse,.style-athelas #rdb-athelas{display:block;}#rdb-footer-print,#readability-url,.rdbTypekit #rdb-footer-right .footer-twitterLink,span.version{display:none;}#readTools{width:34px;height:150px;position:fixed;z-index:100;top:10px;left:10px;}#readTools a{overflow:hidden;margin-bottom:8px;display:block;opacity:.4;text-indent:-99999px;height:34px;width:34px;text-decoration:none;filter:alpha(opacity=40);}#reload-page{background-position:0 0;}#print-page{background-position:-36px 0;}#email-page{background-position:-72px 0;}#kindle-page{background-position:-108px 0;}#readTools a:hover,#readTools a:focus{opacity:1;filter:alpha(opacity=100);}.size-x-small{font-size:1.1em;line-height:1.5em;}.size-small{font-size:1.4em;line-height:1.5em;}.size-medium{font-size:1.7em;line-height:1.48em;}.size-large{font-size:2em;line-height:1.47em;}.size-x-large{font-size:2.5em;line-height:1.43em;}.size-x-small h1,.size-small h1,.size-medium h1,.size-large h1{font-size:1.6em;line-height:1.5em;}.size-x-large h1{font-size:1.6em;line-height:1.3em;}.style-newspaper {font-family:Georgia,\"Times New Roman\", Times, serif;background:#fbfbfb;color:#080000;}.style-newspaper h1 {text-transform:capitalize;font-family:Georgia, \"Times New Roman\", Times, serif;}.style-newspaper #readInner a {color:#0924e1;}.style-novel {font-family:\"Palatino Linotype\", \"Book Antiqua\", Palatino, serif;background:#f4eed9;color:#1d1916;}.style-novel #readInner a {color:#1856ba;}.style-ebook {font-family:Arial, Helvetica, sans-serif;background:#edebe8;color:#2c2d32;}.style-ebook #readInner a {color:#187dc9;}.style-ebook h1 {font-family:\"Arial Black\", Gadget, sans-serif;font-weight:400;}.style-terminal {font-family:\"Lucida Console\", Monaco, monospace;background:#1d4e2c;color:#c6ffc6;}.style-terminal #readInner a {color:#093;}.style-apertura{font-family:apertura-1,apertura-2,sans-serif;background-color:#2d2828;color:#eae8e9;}.style-apertura #readInner a{color:#58b0ff;}.style-athelas{font-family:athelas-1,athelas-2,\"Palatino Linotype\",\"Book Antiqua\",Palatino,serif;background-color:#f7f7f7;color:#2b373d;}.style-athelas #readInner a{color:#1e83cb;}.margin-x-narrow{width:95%;}.margin-narrow{width:85%;}.margin-medium{width:75%;}.margin-wide{width:55%;}.margin-x-wide{width:35%;}.bug-green{background:#bbf9b0;border:4px solid green;}.bug-red{background:red;}.bug-yellow{background:#ffff8e;}.bug-blue{background:#bfdfff;}#kindle-container,#email-container{position:fixed;top:60px;left:50%;width:500px;height:490px;border:solid 3px #666;background-color:#fff;z-index:100!important;overflow:hidden;margin:0 0 0 -240px;padding:0;}table,tr,td{background-color:transparent!important;}";
	_readability_css.type='text/css';
	_readability_css.media='all';
	document.documentElement.appendChild(_readability_css);
	_readability_print_css=document.createElement('style');
	_readability_print_css.rel='stylesheet';
//readability-print.css
	_readability_print_css.text="body, #readOverlay {background-color: white !important;}embed, object {	display: none !important;}#readInner.margin-x-narrow {width:100% !important;}#readInner.margin-narrow {width:91% !important;}#readInner.margin-medium {width:84% !important;}#readInner.margin-wide {width:77% !important;}#readInner.margin-x-wide {width:70% !important;}#readTools {display: none !important;}#readFooter {font-size: .85em;text-align: left !important;}#readFooter a {border-bottom-width: 0 !important;}#readability-url {	display: inline !important;}#rdb-footer-print {display: block !important;font-size: .85em;text-align: right;margin-bottom: 1.5em;}#rdb-footer-print span {white-space: ;}#rdb-footer-left {width: auto !important;display: block !important;float: none !important;font-family: \"Adobe Caslon Pro\", \"Hoefler Text\", Georgia, Garamond, serif;}#rdb-footer-right {display: none !important;}#readFooter #arc90-logo, #readFooter #readability-logo {background-image: none !important;text-indent: 0 !important;width: auto !important;height: auto !important;line-height: auto !important;margin: 0 !important;text-align: left;}#readFooter #readability-logo {color: #333 !important;font-variant: small-caps;}#footer-twitterLink {display: none;}#readFooter span.version {display: none;}a {color: #333 !important;border-bottom: 1px solid #CCC !important;}div.footer-right {display: none;}";
	_readability_print_css.media='print';
	_readability_print_css.type='text/css';
	document.getElementsByTagName('head')[0].appendChild(_readability_print_css);
}
main();
