/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'clash-blue': '#3b82f6',
        'clash-gold': '#fbbf24',
        'clash-dark': '#0f172a',
        'clash-purple': '#a855f7',
      }
    },
  },
  plugins: [],
}
