/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @flow strict-local
 * @format
 */

import type {BundleOptions, BundleOutOptions} from './BuildBundle';

import * as path from 'path';
import yargs from 'yargs';
import {buildBundles} from './BuildBundle';

async function main() {
  const rawArgs = process.argv.slice(2);
  const cliYargs = yargs(rawArgs)
    .usage('Usage: $0 [entrypoint]')
    .scriptName('flow-bundler')
    .option('out', {
      alias: 'o',
      describe: 'Path to output the bundle',
      nargs: 1,
      required: true,
      requiresArg: true,
      type: 'string',
    })
    .option('simple-jsx-transform', {
      describe: 'Basic JSX transform for use with MiniReact',
      default: false,
      type: 'boolean',
    })
    .option('es5', {
      describe:
        'Create ES5 syntax compatible bundle along side standard bundle',
      default: false,
      type: 'boolean',
    })
    .option('strip-types', {
      describe: 'Strip flow types without lowering',
      default: false,
      type: 'boolean',
    })
    .option('root', {
      alias: 'r',
      describe:
        'Root path, paths will be shown relative to this location (default: cwd).',
      nargs: 1,
      requiresArg: true,
      type: 'string',
      default: process.cwd(),
    })
    .option('babel-transforms', {
      alias: 't',
      describe:
        'Babel transforms to apply to the file before writing. Expects a path to a babel config file.',
      type: 'string',
      default: '',
    })
    .positional('entrypoint', {
      type: 'string',
      describe: 'Bundle entrypoint files',
    })
    .wrap(yargs.terminalWidth())
    .parse();

  const outPath = cliYargs.out;
  const outFilename: string = path.basename(outPath);

  const baseBabelConfig = {
    plugins:
      cliYargs.babelTransforms !== ''
        ? // $FlowExpectedError[unsupported-syntax]
          require(cliYargs.babelTransforms)
        : null,
  };

  const bundleOut: {[string]: ?BundleOutOptions} = {
    [outFilename]: {
      babelConfig: baseBabelConfig,
    },
  };

  if (cliYargs.es5) {
    bundleOut[outFilename.replace('.js', '-es5.js')] = {
      babelConfig: {
        presets: [
          [
            require.resolve('metro-react-native-babel-preset'),
            {enableBabelRuntime: false},
          ],
        ],
        plugins: baseBabelConfig.plugins,
      },
    };
  }

  if (cliYargs.stripTypes) {
    const stripTypesTransform = require.resolve(
      '@babel/plugin-transform-flow-strip-types',
    );
    bundleOut[outFilename.replace('.js', '-stripped.js')] = {
      babelConfig: {
        plugins:
          baseBabelConfig.plugins != null
            ? [...baseBabelConfig.plugins, stripTypesTransform]
            : [stripTypesTransform],
      },
    };
  }

  const bundleOptions: BundleOptions = {
    root: cliYargs.root,
    entrypoints: cliYargs._,
    simpleJsxTransform: cliYargs.simpleJsxTransform,
    outDir: path.dirname(outPath),
    out: bundleOut,
  };

  await buildBundles(bundleOptions);
}

main().catch((err: mixed) => {
  console.error(err);
  process.exitCode = 1;
});
