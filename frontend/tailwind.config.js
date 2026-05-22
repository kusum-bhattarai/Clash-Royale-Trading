/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      fontFamily: {
        'display': ['Russo One', 'sans-serif'],
        'sans': ['Rajdhani', 'sans-serif'],
        'mono': ['JetBrains Mono', 'monospace'],
      },
      colors: {
        'clash-blue': '#3b82f6',
        'clash-gold': '#fbbf24',
        'clash-dark': '#0f172a',
        'clash-purple': '#a855f7',
        // Arena palette
        'arena-bg': '#0a0a0f',
        'arena-surface': '#0f1117',
        'arena-border': 'rgba(99,102,241,0.2)',
        'neon-cyan': '#00d4ff',
        'neon-purple': '#a855f7',
        'neon-gold': '#fbbf24',
        'bull-green': '#00ff88',
        'bear-red': '#ff3b5c',
      },
      animation: {
        'ticker': 'ticker 30s linear infinite',
        'pulse-glow': 'pulseGlow 2s ease-in-out infinite',
      },
      keyframes: {
        ticker: {
          '0%': { transform: 'translateX(0%)' },
          '100%': { transform: 'translateX(-50%)' },
        },
        pulseGlow: {
          '0%, 100%': { opacity: '1' },
          '50%': { opacity: '0.6' },
        },
      },
    },
  },
  plugins: [],
}
