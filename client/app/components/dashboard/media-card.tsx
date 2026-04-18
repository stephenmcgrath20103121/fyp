import React from "react";
import Card from "@mui/material/Card";
import CardActions from "@mui/material/CardActions";
import CardContent from "@mui/material/CardContent";
import CardMedia from "@mui/material/CardMedia";
import CardHeader from "@mui/material/CardHeader";
import Button from "@mui/material/Button";
import IconButton from "@mui/material/IconButton";
import Typography from "@mui/material/Typography";
import FavoriteIcon from "@mui/icons-material/Favorite";
import FolderIcon from "@mui/icons-material/Folder";
import TimerIcon from "@mui/icons-material/Timer";
import DeleteIcon from '@mui/icons-material/Delete'
import PlayIcon from '@mui/icons-material/PlayCircle'
import Grid from "@mui/material/Grid";
import Link from "next/link";
import Avatar from '@mui/material/Avatar';
import img from '../../../public/placeholder.png'

export default function mediaCard() {
  return (
    <Card sx={{ maxWidth: 200, maxHeight: 500, mt: 1.5, mb: 1.5, bgcolor: 'primary.main' }}>
      <CardHeader
        avatar={
            <Avatar sx={{ backgroundColor: 'lightcoral', maxWidth: 24, maxHeight: 24 }}>
              <FavoriteIcon sx={{color: 'primary.contrastText', maxWidth: 16, maxHeight: 16}} />
            </Avatar>
        }
        title={
          <Typography variant="h6" component="p" sx={{ color: 'primary.contrastText', maxWidth: 225, mt:1 }}>
            Title
          </Typography>
        }
        sx={{ maxHeight: 75, color: 'tertiary.contrastText'}}
      />

      <CardMedia
        sx={{ width: 150, height: 150, ml: 3, mr: 3, mb: .5, border: 'solid', borderColor: 'tertiary.main', borderRadius: 1 }}
        image={
          'file.svg'
        }
      />
      <CardContent sx={{bgcolor: 'tertiary.light'}} >
        <Grid container sx={{bgcolor: 'primary.main', color: 'primary.contrastText', borderRadius: 2, pl: 1, pr: 1, pt: .5}}>
          <Grid size={{xs: 8}} sx={{mb:1}}>
            <Typography variant="body1" component="p" sx={{fontSize: 14}}>
              <TimerIcon sx={{maxWidth: 14, maxHeight: 14, mr: .5, mb: .5}} />
              120 mins
            </Typography>
          </Grid>
          <Grid size={{xs: 8}} sx={{mb:1}}>
            <Typography variant="body1" component="p" sx={{fontSize: 14}}>
              <FolderIcon sx={{maxWidth: 14, maxHeight: 14, mr: .5, mb: .5}} />
              Categories: 
            </Typography>
          </Grid>
        </Grid>
      </CardContent>
      <CardActions disableSpacing>
      
        <Link href={`/media/1`}>
          <Button variant="contained" size="small" startIcon={<PlayIcon />} sx={{bgcolor: "secondary.main", color: "primary.contrastText", ml: .5,mb: 1, pl: 3.5, pr: 3.5}}>
            View
          </Button>
        </Link>
        <Link href={`/media/1`}>
          <IconButton size="small" sx={{bgcolor: "secondary.main", color: "primary.contrastText", ml: 2.5, mb: 1, mr: .5}}>
            <DeleteIcon />
          </IconButton>
        </Link>
        
      </CardActions>
    </Card>
  );
}