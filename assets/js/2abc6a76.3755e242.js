(self.webpackChunkhermes_website=self.webpackChunkhermes_website||[]).push([[239],{3905:function(e,t,n){"use strict";n.d(t,{Zo:function(){return p},kt:function(){return h}});var o=n(7294);function i(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function r(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var o=Object.getOwnPropertySymbols(e);t&&(o=o.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,o)}return n}function a(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?r(Object(n),!0).forEach((function(t){i(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):r(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function s(e,t){if(null==e)return{};var n,o,i=function(e,t){if(null==e)return{};var n,o,i={},r=Object.keys(e);for(o=0;o<r.length;o++)n=r[o],t.indexOf(n)>=0||(i[n]=e[n]);return i}(e,t);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);for(o=0;o<r.length;o++)n=r[o],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(i[n]=e[n])}return i}var l=o.createContext({}),c=function(e){var t=o.useContext(l),n=t;return e&&(n="function"==typeof e?e(t):a(a({},t),e)),n},p=function(e){var t=c(e.components);return o.createElement(l.Provider,{value:t},e.children)},m={inlineCode:"code",wrapper:function(e){var t=e.children;return o.createElement(o.Fragment,{},t)}},u=o.forwardRef((function(e,t){var n=e.components,i=e.mdxType,r=e.originalType,l=e.parentName,p=s(e,["components","mdxType","originalType","parentName"]),u=c(n),h=i,d=u["".concat(l,".").concat(h)]||u[h]||m[h]||r;return n?o.createElement(d,a(a({ref:t},p),{},{components:n})):o.createElement(d,a({ref:t},p))}));function h(e,t){var n=arguments,i=t&&t.mdxType;if("string"==typeof e||i){var r=n.length,a=new Array(r);a[0]=u;var s={};for(var l in t)hasOwnProperty.call(t,l)&&(s[l]=t[l]);s.originalType=e,s.mdxType="string"==typeof e?e:i,a[1]=s;for(var c=2;c<r;c++)a[c]=n[c];return o.createElement.apply(null,a)}return o.createElement.apply(null,n)}u.displayName="MDXCreateElement"},8442:function(e,t,n){"use strict";n.r(t),n.d(t,{frontMatter:function(){return a},metadata:function(){return s},toc:function(){return l},default:function(){return p}});var o=n(4034),i=n(9973),r=(n(7294),n(3905)),a={id:"optimizer",title:"Design of the Optimizer"},s={unversionedId:"optimizer",id:"optimizer",isDocsHomePage:!1,title:"Design of the Optimizer",description:"Introduction",source:"@site/../doc/Optimizer.md",sourceDirName:".",slug:"/optimizer",permalink:"/docs/optimizer",editUrl:"https://github.com/facebook/hermes/blob/HEAD/website/../doc/Optimizer.md",version:"current",lastUpdatedAt:1607758573,formattedLastUpdatedAt:"12/11/2020",frontMatter:{id:"optimizer",title:"Design of the Optimizer"},sidebar:"docs",previous:{title:"Design of the IR",permalink:"/docs/ir"},next:{title:"VM Overview",permalink:"/docs/vm"}},l=[{value:"Introduction",id:"introduction",children:[]},{value:"Key concepts",id:"key-concepts",children:[]}],c={toc:l};function p(e){var t=e.components,n=(0,i.Z)(e,["components"]);return(0,r.kt)("wrapper",(0,o.Z)({},c,n,{components:t,mdxType:"MDXLayout"}),(0,r.kt)("h3",{id:"introduction"},"Introduction"),(0,r.kt)("p",null,"This document describes the high-level design of the Hermes optimizer. The\nHermes optimizer transforms the Hermes IR into a more efficient representation\nthat preserves the original semantics of the program. The IR.md document describes\nthe design of the Hermes IR."),(0,r.kt)("h3",{id:"key-concepts"},"Key concepts"),(0,r.kt)("p",null,"This section describes a few key concepts and ideas:"),(0,r.kt)("ul",null,(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},"The optimizer is responsible for optimizing the IR. IRGen and BytecodeGen\nare not the right place for implementing optimizations. The parts of the\ncompiler that translate from one representation to another are inherently\ncomplex because they require the understanding of the semantics of both\nrepresentations. Moreover, translators are not designed like optimizers.\nThey do not have good access to analysis and do not allow the separation\nof the optimizer from the translation, which makes debugging more\ndifficult.")),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},"Passes: Optimizations are organized in passes. There are two kinds of\npasses: function passes and module passes. Function passes can modify only\nthe functions that they operate on, while module passes operate on the whole\nmodule.  Function passes are allowed to read the whole module but only\ntouch the current function.")),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},"Analysis: Analyses are caches in front of a computation of some property.\nFor example, the dominator analysis is a cache that helps reduce compile\ntime by removing the need to recompute the dominator tree for each\nfunction. Analyses are all about caching and invalidating pre-computed\nproperties.")),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},'Optimizations do one thing: Optimizations are designed to be simple and\nthis means that they do only one thing. For example, the common\nsubexpression elimination optimization does not delete dead code\n"on the way" just because it can.')),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},"Optimizations are predictable: Sometimes there are several legal\nrepresentations of the program, but the optimizer should never\nrandomize the output of the compiler. Randomization of the output\nhappens when the output depends on runtime information such as the order\nof elements in a set or map. Randomizing the output of the compiler\nmakes it very difficult to write tests and reproduce bugs. LLVM has\ndata structures that provide guaranteed order - use them!")),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},'Write compile-time efficient algorithms: The compile time of a compiler is a\nvery important metric and we attempt to minimize compile time as much as\npossible. Do not write exponential algorithms (or polynomial algorithm with\na high degree). If you are writing a quadratic algorithm make sure to\nimplement a sliding-window or other techniques that will allows to limit\nthe quadratic search to a small subset of the graph. Always assume that\nthere exist a function with hundreds of consecutive basic blocks or a basic\nblock with thousands of instructions. If you are writing a "solver" then\nyou are probably doing it wrong.')),(0,r.kt)("li",{parentName:"ul"},(0,r.kt)("p",{parentName:"li"},"There are three kinds of transformations: canonicalization,\nsimplification and lowering. Make sure that you know exactly what kind of\ntransformation you are doing and why. Canonicalizations are transformations that\nexpose opportunities for other transformations.  Re-association (reducing tree\nheight, placing constants on the RHS, etc.) is a canonicalization because it\norganizes things in predictable patterns and makes the life of future\noptimizations simpler by reducing the number of possible inputs. Inlining is\nanother example of effective canonicalization because it exposes opportunities\nfor optimizations in the caller function (by providing more information).\nAnother example is loop rotation, which is a canonical representation of all\nloops.  In canonicalization we strive to clean up the program as much as\npossible and reach a pure representation of the program.  Simplification is what\nwe normally think of as optimizations, like removing redundancy by deleting dead\ncode and optimizing arithmetic, etc.  Canonicalization can allow simplification\nthat can allow more canonicalization.  For example, de-virtualization unblocks\ninlining that may allow some transformations that enable more de-virtualization.\nLowering transformations are the opposite of canonicalization. In Lowering\ntransformations we generate patterns that are closer to the target\nrepresentation. We may not be able to recover from lowering transformations. One\nexample for lowering transformation is loop strength reduction where the\noptimizer transforms loop indices into non-consecutive accesses that fit with\nthe hardware instruction set. Another example is loop versioning where the body\nof the loop is duplicated and versioned multiple times ."))))}p.isMDXComponent=!0}}]);