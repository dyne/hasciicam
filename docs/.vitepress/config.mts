import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Hasciicam',
  description: 'Live ASCII video for the web',
  base: process.env.BASE_PATH ?? '/',
  appearance: true,
  themeConfig: {
    nav: [
      { text: 'Usage guide', link: '/usage-guide' },
      { text: 'Downloads', link: 'https://files.dyne.org/hasciicam' },
      { text: 'About Dyne.org', link: 'https://dyne.org' }
    ],
    socialLinks: [
      { icon: 'github', link: 'https://github.com/jaromil/hasciicam/' }
    ],

    sidebar: [
      {
        text: 'Hasciicam',
        items: [
          { text: 'Home', link: '/' },
          { text: 'Usage guide', link: '/usage-guide' },
          { text: 'Manual page', link: '/manpage.html' },
          { text: 'Source code', link: 'https://github.com/jaromil/hasciicam/' }
        ]
      }
    ]
  }
})
