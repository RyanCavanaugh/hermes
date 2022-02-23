(self.webpackChunkhermes_website=self.webpackChunkhermes_website||[]).push([[355],{3905:function(e,t,n){"use strict";n.d(t,{Zo:function(){return c},kt:function(){return d}});var i=n(7294);function r(e,t,n){return t in e?Object.defineProperty(e,t,{value:n,enumerable:!0,configurable:!0,writable:!0}):e[t]=n,e}function o(e,t){var n=Object.keys(e);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(e);t&&(i=i.filter((function(t){return Object.getOwnPropertyDescriptor(e,t).enumerable}))),n.push.apply(n,i)}return n}function a(e){for(var t=1;t<arguments.length;t++){var n=null!=arguments[t]?arguments[t]:{};t%2?o(Object(n),!0).forEach((function(t){r(e,t,n[t])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(n)):o(Object(n)).forEach((function(t){Object.defineProperty(e,t,Object.getOwnPropertyDescriptor(n,t))}))}return e}function l(e,t){if(null==e)return{};var n,i,r=function(e,t){if(null==e)return{};var n,i,r={},o=Object.keys(e);for(i=0;i<o.length;i++)n=o[i],t.indexOf(n)>=0||(r[n]=e[n]);return r}(e,t);if(Object.getOwnPropertySymbols){var o=Object.getOwnPropertySymbols(e);for(i=0;i<o.length;i++)n=o[i],t.indexOf(n)>=0||Object.prototype.propertyIsEnumerable.call(e,n)&&(r[n]=e[n])}return r}var s=i.createContext({}),p=function(e){var t=i.useContext(s),n=t;return e&&(n="function"==typeof e?e(t):a(a({},t),e)),n},c=function(e){var t=p(e.components);return i.createElement(s.Provider,{value:t},e.children)},m={inlineCode:"code",wrapper:function(e){var t=e.children;return i.createElement(i.Fragment,{},t)}},u=i.forwardRef((function(e,t){var n=e.components,r=e.mdxType,o=e.originalType,s=e.parentName,c=l(e,["components","mdxType","originalType","parentName"]),u=p(n),d=r,h=u["".concat(s,".").concat(d)]||u[d]||m[d]||o;return n?i.createElement(h,a(a({ref:t},c),{},{components:n})):i.createElement(h,a({ref:t},c))}));function d(e,t){var n=arguments,r=t&&t.mdxType;if("string"==typeof e||r){var o=n.length,a=new Array(o);a[0]=u;var l={};for(var s in t)hasOwnProperty.call(t,s)&&(l[s]=t[s]);l.originalType=e,l.mdxType="string"==typeof e?e:r,a[1]=l;for(var p=2;p<o;p++)a[p]=n[p];return i.createElement.apply(null,a)}return i.createElement.apply(null,n)}u.displayName="MDXCreateElement"},7766:function(e,t,n){"use strict";n.r(t),n.d(t,{frontMatter:function(){return a},metadata:function(){return l},toc:function(){return s},default:function(){return c}});var i=n(4034),r=n(9973),o=(n(7294),n(3905)),a={id:"emscripten",title:"Building with Emscripten"},l={unversionedId:"emscripten",id:"emscripten",isDocsHomePage:!1,title:"Building with Emscripten",description:"Setting up Emscripten",source:"@site/../doc/Emscripten.md",sourceDirName:".",slug:"/emscripten",permalink:"/docs/emscripten",editUrl:"https://github.com/facebook/hermes/blob/HEAD/website/../doc/Emscripten.md",version:"current",lastUpdatedAt:1620193287,formattedLastUpdatedAt:"5/4/2021",frontMatter:{id:"emscripten",title:"Building with Emscripten"},sidebar:"docs",previous:{title:"Building and Running",permalink:"/docs/building-and-running"},next:{title:"Cross Compilation",permalink:"/docs/cross-compilation"}},s=[{value:"Setting up Emscripten",id:"setting-up-emscripten",children:[]},{value:"Setting up Workspace and Host Hermesc",id:"setting-up-workspace-and-host-hermesc",children:[]},{value:"Building Hermes With configure.py",id:"building-hermes-with-configurepy",children:[]},{value:"Build with CMake directly",id:"build-with-cmake-directly",children:[]}],p={toc:s};function c(e){var t=e.components,n=(0,r.Z)(e,["components"]);return(0,o.kt)("wrapper",(0,i.Z)({},p,n,{components:t,mdxType:"MDXLayout"}),(0,o.kt)("h2",{id:"setting-up-emscripten"},"Setting up Emscripten"),(0,o.kt)("p",null,"To setup Emscripten for building Hermes, we recommend using ",(0,o.kt)("inlineCode",{parentName:"p"},"emsdk"),", which is\nthe same way Emscripten recommends for most circumstances.\nFollow the directions on the\n",(0,o.kt)("a",{parentName:"p",href:"https://emscripten.org/docs/getting_started/downloads.html"},"Emscripten website for ",(0,o.kt)("inlineCode",{parentName:"a"},"emsdk")),"\nto download the SDK."),(0,o.kt)("pre",null,(0,o.kt)("code",{parentName:"pre"},"emsdk install latest\nemsdk activate latest\nsource ./emsdk_env.sh\n")),(0,o.kt)("p",null,"If you install ",(0,o.kt)("inlineCode",{parentName:"p"},"emsdk")," at ",(0,o.kt)("inlineCode",{parentName:"p"},"~/emsdk")," and activate ",(0,o.kt)("inlineCode",{parentName:"p"},"latest"),",\nthen you should use this shell variable for the rest of these instructions:"),(0,o.kt)("pre",null,(0,o.kt)("code",{parentName:"pre"},"$EmscriptenRoot = ~/emsdk/upstream/emscripten\n")),(0,o.kt)("p",null,"If you are using the old ",(0,o.kt)("inlineCode",{parentName:"p"},"fastcomp")," instead, replace ",(0,o.kt)("inlineCode",{parentName:"p"},"upstream")," in the above instruction with ",(0,o.kt)("inlineCode",{parentName:"p"},"fastcomp"),"."),(0,o.kt)("p",null,"WARNING: The old ",(0,o.kt)("inlineCode",{parentName:"p"},"fastcomp")," backend was ",(0,o.kt)("a",{parentName:"p",href:"https://emscripten.org/docs/compiling/WebAssembly.html?highlight=fastcomp#backends"},"removed in emscripten ",(0,o.kt)("inlineCode",{parentName:"a"},"2.0.0")," (August 2020)")),(0,o.kt)("h2",{id:"setting-up-workspace-and-host-hermesc"},"Setting up Workspace and Host Hermesc"),(0,o.kt)("p",null,"Hermes now requires a two stage build proecess because the VM now contains\nHermes bytecode which needs to be compiled by Hermes."),(0,o.kt)("p",null,"Please follow the ",(0,o.kt)("a",{parentName:"p",href:"/docs/cross-compilation"},"Cross Compilation")," to set up a workplace\nand build a host hermesc at ",(0,o.kt)("inlineCode",{parentName:"p"},"$HERMES_WS_DIR/build_host_hermesc"),"."),(0,o.kt)("h2",{id:"building-hermes-with-configurepy"},"Building Hermes With configure.py"),(0,o.kt)("pre",null,(0,o.kt)("code",{parentName:"pre"},'# Configure the build. Here the build is output to a\n# directory starting with the prefix "embuild".\npython3 ${HERMES_WS_DIR}/hermes/utils/build/configure.py \\\n    --cmake-flags " -DIMPORT_HERMESC:PATH=${HERMES_WS_DIR}/build_host_hermesc/ImportHermesc.cmake " \\\n    --distribute \\\n    --wasm \\\n    --emscripten-platform=upstream \\\n    --emscripten-root="${EmscriptenRoot?}" \\\n    /tmp/embuild\n\n# Build Hermes. The build directory name will depend on the flags passed to\n# configure.py.\ncmake --build /tmp/embuild --target hermes\n# Execute hermes\nnode /tmp/embuild/bin/hermes.js --help\n')),(0,o.kt)("p",null,"Make sure that the ",(0,o.kt)("inlineCode",{parentName:"p"},"--emscripten-platform")," option matches the directory given\nto ",(0,o.kt)("inlineCode",{parentName:"p"},"--emscripten-root"),", and is also the current activated Emscripten toolchain\nvia ",(0,o.kt)("inlineCode",{parentName:"p"},"emsdk activate"),"."),(0,o.kt)("p",null,"See ",(0,o.kt)("inlineCode",{parentName:"p"},"configure.py --help")," for more build options."),(0,o.kt)("h2",{id:"build-with-cmake-directly"},"Build with CMake directly"),(0,o.kt)("p",null,"The ",(0,o.kt)("inlineCode",{parentName:"p"},"configure.py")," script runs CMake for you with options chosen by the Hermes\nproject. If you want to customize your build, you can take this command as a\nbase."),(0,o.kt)("pre",null,(0,o.kt)("code",{parentName:"pre"},'cmake ${HERMES_WS_DIR}/hermes \\\n        -B embuild \\\n        -DCMAKE_TOOLCHAIN_FILE=${EmscriptenRoot?}/cmake/Modules/Platform/Emscripten.cmake \\\n        -DCMAKE_BUILD_TYPE=Release \\\n        -DCMAKE_EXE_LINKER_FLAGS="-s NODERAWFS=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1" \\\n        -DIMPORT_HERMESC:PATH="${HERMES_WS_DIR}/build_host_hermesc/ImportHermesc.cmake"\n')),(0,o.kt)("p",null,"Each option is explained below:"),(0,o.kt)("ul",null,(0,o.kt)("li",{parentName:"ul"},(0,o.kt)("inlineCode",{parentName:"li"},"CMAKE_BUILD_TYPE"),": set it to one of CMake's build modes: ",(0,o.kt)("inlineCode",{parentName:"li"},"Debug"),", ",(0,o.kt)("inlineCode",{parentName:"li"},"Release"),",\n",(0,o.kt)("inlineCode",{parentName:"li"},"MinSizeRel"),", etc."),(0,o.kt)("li",{parentName:"ul"},(0,o.kt)("inlineCode",{parentName:"li"},"EMSCRIPTEN_FASTCOMP"),": set to ",(0,o.kt)("inlineCode",{parentName:"li"},"1")," if using fastcomp, or ",(0,o.kt)("inlineCode",{parentName:"li"},"0")," if using upstream\n(LLVM)"),(0,o.kt)("li",{parentName:"ul"},(0,o.kt)("inlineCode",{parentName:"li"},"WASM"),": whether to use asm.js (",(0,o.kt)("inlineCode",{parentName:"li"},"0"),"), WebAssembly (",(0,o.kt)("inlineCode",{parentName:"li"},"1"),"), or both (",(0,o.kt)("inlineCode",{parentName:"li"},"2"),")"),(0,o.kt)("li",{parentName:"ul"},(0,o.kt)("inlineCode",{parentName:"li"},"NODERAWFS"),": set to ",(0,o.kt)("inlineCode",{parentName:"li"},"1")," if you will be running Hermes directly with Node. It\nenables direct access to the filesystem."),(0,o.kt)("li",{parentName:"ul"},(0,o.kt)("inlineCode",{parentName:"li"},"ALLOW_MEMORY_GROWTH"),": whether to pre-allocate all memory, or let it grow over\ntime")),(0,o.kt)("p",null,"You can customize the build generator by passing the ",(0,o.kt)("inlineCode",{parentName:"p"},"-G")," option to CMake, for\nexample ",(0,o.kt)("inlineCode",{parentName:"p"},"-G Ninja"),"."))}c.isMDXComponent=!0}}]);