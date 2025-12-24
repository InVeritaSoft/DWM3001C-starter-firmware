#!/usr/bin/env node
/**
 * Baud Rate and Hex Data Diagnostic Tool
 * Tests different baud rates and analyzes hex data corruption
 */

import { SerialPort } from 'serialport';
import { ReadlineParser } from 'serialport';
import { setTimeout as sleep } from 'timers/promises';

const BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000];
const TEST_PORTS = process.argv.slice(2);

if (TEST_PORTS.length === 0) {
  console.error('Usage: node scripts/diagnose-baud-hex.js /dev/ttyUSB0 [/dev/ttyUSB1]');
  process.exit(1);
}

function analyzeHexData(hexString) {
  const analysis = {
    isCorrupted: false,
    isPrintable: false,
    patterns: [],
    suggestions: []
  };

  // Check if it's mostly printable ASCII
  const bytes = hexString.match(/.{2}/g) || [];
  const printableCount = bytes.filter(b => {
    const val = parseInt(b, 16);
    return (val >= 0x20 && val <= 0x7E) || val === 0x0D || val === 0x0A;
  }).length;
  
  analysis.isPrintable = printableCount / bytes.length > 0.7;
  
  // Check for common corruption patterns
  const repeatingPatterns = hexString.match(/(.{2})\1{3,}/g);
  if (repeatingPatterns) {
    analysis.patterns.push(`Repeating bytes: ${repeatingPatterns.join(', ')}`);
    analysis.isCorrupted = true;
  }
  
  // Check for all high bits (0x80-0xFF) - common in baud mismatch
  const highBitCount = bytes.filter(b => parseInt(b, 16) >= 0x80).length;
  if (highBitCount / bytes.length > 0.5) {
    analysis.patterns.push('Many high-bit values (0x80-0xFF) - suggests baud rate mismatch');
    analysis.isCorrupted = true;
  }
  
  // Check for alternating patterns (common in wrong baud)
  if (hexString.match(/f[0-9a-f]f[0-9a-f]f[0-9a-f]/i)) {
    analysis.patterns.push('Alternating high/low pattern - strong indicator of baud mismatch');
    analysis.isCorrupted = true;
  }
  
  return analysis;
}

function hexToAscii(hexString) {
  const bytes = hexString.match(/.{2}/g) || [];
  return bytes.map(b => {
    const val = parseInt(b, 16);
    if (val >= 0x20 && val <= 0x7E) {
      return String.fromCharCode(val);
    } else if (val === 0x0D) return '\\r';
    else if (val === 0x0A) return '\\n';
    else if (val === 0x00) return '\\0';
    else return '.';
  }).join('');
}

async function testBaudRate(portPath, baudRate) {
  return new Promise((resolve) => {
    const port = new SerialPort({
      path: portPath,
      baudRate,
      dataBits: 8,
      parity: 'none',
      stopBits: 1,
      autoOpen: false,
    });

    const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));
    let receivedData = [];
    let rawHexData = [];
    let timeout;

    // Capture raw data
    port.on('data', (data) => {
      const hex = data.toString('hex');
      if (hex.length > 0) {
        rawHexData.push(hex);
      }
    });

    parser.on('data', (data) => {
      const trimmed = data.toString().trim();
      if (trimmed.length > 0) {
        receivedData.push(trimmed);
      }
    });

    port.on('open', async () => {
      // Wait 2 seconds for initialization messages
      await sleep(2000);
      
      // Send test command
      port.write('PNG\r\n');
      
      // Wait for response
      timeout = setTimeout(() => {
        port.close();
        resolve({ 
          receivedData, 
          rawHexData: rawHexData.join(''),
          baudRate 
        });
      }, 2000);
    });

    port.on('error', (err) => {
      clearTimeout(timeout);
      port.close();
      resolve({ error: err.message, baudRate });
    });

    port.open();
  });
}

async function testPort(portPath) {
  console.log(`\n${'='.repeat(60)}`);
  console.log(`Testing ${portPath}`);
  console.log('='.repeat(60));

  const results = [];

  for (const baudRate of BAUD_RATES) {
    process.stdout.write(`Testing ${baudRate.toString().padStart(7)} baud... `);
    const result = await testBaudRate(portPath, baudRate);
    
    if (result.error) {
      console.log(`❌ Error: ${result.error}`);
      results.push({ baudRate, status: 'error', error: result.error });
    } else {
      const hasData = result.receivedData.length > 0 || result.rawHexData.length > 0;
      
      if (hasData) {
        // Analyze hex data
        const analysis = analyzeHexData(result.rawHexData);
        
        // Check for firmware messages
        const hasFirmwareMsg = result.receivedData.some(msg => 
          msg.includes('Firmware') || 
          msg.includes('UART0') || 
          msg.includes('initialized') ||
          msg.includes('OK PONG') ||
          msg.includes('Node Type')
        );
        
        if (hasFirmwareMsg && !analysis.isCorrupted) {
          console.log(`✅ GOOD - Firmware messages detected`);
          results.push({ baudRate, status: 'good', data: result.receivedData, hex: result.rawHexData });
        } else if (hasFirmwareMsg && analysis.isCorrupted) {
          console.log(`⚠️  PARTIAL - Firmware messages but corrupted hex`);
          results.push({ baudRate, status: 'partial', data: result.receivedData, hex: result.rawHexData, analysis });
        } else if (analysis.isCorrupted) {
          console.log(`❌ CORRUPTED - Hex data shows corruption patterns`);
          results.push({ baudRate, status: 'corrupted', hex: result.rawHexData, analysis });
        } else {
          console.log(`⚠️  UNKNOWN - Data received but unclear`);
          results.push({ baudRate, status: 'unknown', data: result.receivedData, hex: result.rawHexData });
        }
      } else {
        console.log(`⚪ NO DATA`);
        results.push({ baudRate, status: 'nodata' });
      }
    }
    
    await sleep(300); // Brief pause between tests
  }

  // Detailed analysis of results
  console.log(`\n${'='.repeat(60)}`);
  console.log(`Detailed Analysis for ${portPath}`);
  console.log('='.repeat(60));

  const goodResults = results.filter(r => r.status === 'good');
  const corruptedResults = results.filter(r => r.status === 'corrupted' || r.status === 'partial');

  if (goodResults.length > 0) {
    console.log(`\n✅ WORKING BAUD RATES (${goodResults.length}):`);
    goodResults.forEach(r => {
      console.log(`\n  ${r.baudRate} baud:`);
      r.data.slice(0, 5).forEach((msg, i) => {
        console.log(`    [${i + 1}] ${msg.substring(0, 70)}`);
      });
    });
  }

  if (corruptedResults.length > 0) {
    console.log(`\n❌ CORRUPTED DATA (${corruptedResults.length} baud rates):`);
    corruptedResults.forEach(r => {
      console.log(`\n  ${r.baudRate} baud:`);
      if (r.analysis) {
        console.log(`    Patterns: ${r.analysis.patterns.join('; ')}`);
      }
      const hexSample = r.hex.substring(0, 80);
      console.log(`    Hex sample: ${hexSample}${r.hex.length > 80 ? '...' : ''}`);
      const asciiSample = hexToAscii(hexSample);
      console.log(`    ASCII attempt: ${asciiSample}`);
    });
  }

  if (goodResults.length === 0 && corruptedResults.length > 0) {
    console.log(`\n⚠️  WARNING: No clean data found, but corruption detected at multiple baud rates.`);
    console.log(`   This suggests:`);
    console.log(`   1. Firmware may not be running/flashed correctly`);
    console.log(`   2. UART pins may be wrong`);
    console.log(`   3. RS-485 wiring issue`);
  }

  return results;
}

async function main() {
  console.log('Baud Rate and Hex Data Diagnostic Tool');
  console.log('======================================\n');
  console.log('This tool will:');
  console.log('  1. Test multiple baud rates');
  console.log('  2. Analyze hex data for corruption patterns');
  console.log('  3. Identify the correct baud rate\n');

  const allResults = {};
  
  for (const port of TEST_PORTS) {
    const results = await testPort(port);
    allResults[port] = results;
  }

  // Summary
  console.log(`\n${'='.repeat(60)}`);
  console.log('SUMMARY & RECOMMENDATIONS');
  console.log('='.repeat(60));

  for (const [port, results] of Object.entries(allResults)) {
    const good = results.filter(r => r.status === 'good');
    const corrupted = results.filter(r => r.status === 'corrupted' || r.status === 'partial');
    
    console.log(`\n${port}:`);
    if (good.length > 0) {
      console.log(`  ✅ Found ${good.length} working baud rate(s): ${good.map(r => r.baudRate).join(', ')}`);
      console.log(`  → Update config/settings.yaml: baudrate: ${good[0].baudRate}`);
    } else if (corrupted.length > 0) {
      console.log(`  ❌ Only corrupted data found at ${corrupted.length} baud rate(s)`);
      console.log(`  → Check firmware, UART pins, or RS-485 wiring`);
    } else {
      console.log(`  ⚪ No data received at any baud rate`);
      console.log(`  → Verify firmware is flashed and boards are powered`);
    }
  }
}

main().catch(console.error);

