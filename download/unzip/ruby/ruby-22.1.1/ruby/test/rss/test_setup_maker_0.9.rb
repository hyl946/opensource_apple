require "rss-testcase"

require "rss/maker"

module RSS
  class TestSetupMaker09 < TestCase

    def test_setup_maker_channel
      title = "fugafuga"
      link = "http://hoge.com"
      description = "fugafugafugafuga"
      language = "ja"
      copyright = "foo"
      managingEditor = "bar"
      webMaster = "web master"
      rating = "6"
      docs = "http://foo.com/doc"
      skipDays = [
        "Sunday",
        "Monday",
      ]
      skipHours = [
        0,
        13,
      ]
      pubDate = Time.now
      lastBuildDate = Time.now

      rss = RSS::Maker.make("0.91") do |maker|
        maker.channel.title = title
        maker.channel.link = link
        maker.channel.description = description
        maker.channel.language = language
        maker.channel.copyright = copyright
        maker.channel.managingEditor = managingEditor
        maker.channel.webMaster = webMaster
        maker.channel.rating = rating
        maker.channel.docs = docs
        maker.channel.pubDate = pubDate
        maker.channel.lastBuildDate = lastBuildDate

        skipDays.each do |day|
          new_day = maker.channel.skipDays.new_day
          new_day.content = day
        end
        skipHours.each do |hour|
          new_hour = maker.channel.skipHours.new_hour
          new_hour.content = hour
        end
      end

      new_rss = RSS::Maker.make("0.91") do |maker|
        rss.channel.setup_maker(maker)
      end
      channel = new_rss.channel
      
      assert_equal(title, channel.title)
      assert_equal(link, channel.link)
      assert_equal(description, channel.description)
      assert_equal(language, channel.language)
      assert_equal(copyright, channel.copyright)
      assert_equal(managingEditor, channel.managingEditor)
      assert_equal(webMaster, channel.webMaster)
      assert_equal(rating, channel.rating)
      assert_equal(docs, channel.docs)
      assert_equal(pubDate, channel.pubDate)
      assert_equal(lastBuildDate, channel.lastBuildDate)

      skipDays.each_with_index do |day, i|
        assert_equal(day, channel.skipDays.days[i].content)
      end
      skipHours.each_with_index do |hour, i|
        assert_equal(hour, channel.skipHours.hours[i].content)
      end
      
      assert(channel.items.empty?)
      assert_nil(channel.image)
      assert_nil(channel.textInput)
    end

    def test_setup_maker_image
      title = "fugafuga"
      link = "http://hoge.com"
      url = "http://hoge.com/hoge.png"
      width = 144
      height = 400
      description = "an image"

      rss = RSS::Maker.make("0.91") do |maker|
        setup_dummy_channel(maker)
        maker.channel.link = link
        
        maker.image.title = title
        maker.image.url = url
        maker.image.width = width
        maker.image.height = height
        maker.image.description = description
      end
      
      new_rss = RSS::Maker.make("0.91") do |maker|
        rss.channel.setup_maker(maker)
        rss.image.setup_maker(maker)
      end
      
      image = new_rss.image
      assert_equal(title, image.title)
      assert_equal(link, image.link)
      assert_equal(url, image.url)
      assert_equal(width, image.width)
      assert_equal(height, image.height)
      assert_equal(description, image.description)
    end
    
    def test_setup_maker_textinput
      title = "fugafuga"
      description = "text hoge fuga"
      name = "hoge"
      link = "http://hoge.com"

      rss = RSS::Maker.make("0.91") do |maker|
        setup_dummy_channel(maker)

        maker.textinput.title = title
        maker.textinput.description = description
        maker.textinput.name = name
        maker.textinput.link = link
      end

      new_rss = RSS::Maker.make("0.91") do |maker|
        rss.channel.setup_maker(maker)
        rss.textinput.setup_maker(maker)
      end
      
      textInput = new_rss.channel.textInput
      assert_equal(title, textInput.title)
      assert_equal(description, textInput.description)
      assert_equal(name, textInput.name)
      assert_equal(link, textInput.link)
    end

    def test_setup_maker_items
      title = "TITLE"
      link = "http://hoge.com/"
      description = "text hoge fuga"

      item_size = 5
      
      rss = RSS::Maker.make("0.91") do |maker|
        setup_dummy_channel(maker)
        
        item_size.times do |i|
          item = maker.items.new_item
          item.title = "#{title}#{i}"
          item.link = "#{link}#{i}"
          item.description = "#{description}#{i}"
        end
      end
      
      new_rss = RSS::Maker.make("0.91") do |maker|
        rss.channel.setup_maker(maker)

        rss.items.each do |item|
          item.setup_maker(maker)
        end
      end
      
      assert_equal(item_size, new_rss.items.size)
      new_rss.items.each_with_index do |item, i|
        assert_equal("#{title}#{i}", item.title)
        assert_equal("#{link}#{i}", item.link)
        assert_equal("#{description}#{i}", item.description)
      end

    end

    def test_setup_maker
      encoding = "EUC-JP"
      standalone = true
      
      href = 'a.xsl'
      type = 'text/xsl'
      title = 'sample'
      media = 'printer'
      charset = 'UTF-8'
      alternate = 'yes'

      rss = RSS::Maker.make("0.91") do |maker|
        maker.encoding = encoding
        maker.standalone = standalone

        xss = maker.xml_stylesheets.new_xml_stylesheet
        xss.href = href
        xss.type = type
        xss.title = title
        xss.media = media
        xss.charset = charset
        xss.alternate = alternate
        
        setup_dummy_channel(maker)
      end
      
      new_rss = RSS::Maker.make("0.91") do |maker|
        rss.setup_maker(maker)
      end
      
      assert_equal("0.91", new_rss.rss_version)
      assert_equal(encoding, new_rss.encoding)
      assert_equal(standalone, new_rss.standalone)

      xss = rss.xml_stylesheets.first
      assert_equal(1, rss.xml_stylesheets.size)
      assert_equal(href, xss.href)
      assert_equal(type, xss.type)
      assert_equal(title, xss.title)
      assert_equal(media, xss.media)
      assert_equal(charset, xss.charset)
      assert_equal(alternate, xss.alternate)
    end
    
  end
end
